#include "taskbar.h"

#define SJ_IMPL
#include "sj.h"

// Bunch of nonsense to avoid multiple-definition compile errors
// When using this library with Raylib, which also uses STB_IMAGE_IMPLEMENTATION.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
	#define STB_IMAGE_IMPLEMENTATION
	#define STB_IMAGE_STATIC
	#include "stb_image.h"
#pragma GCC diagnostic pop

#define BACKGROUND_COLOR swr_rgb(42, 44, 46)
#define TEXT_COLOR swr_rgb(220, 220, 220)

#define MAX(a, b) ((a) > (b) ? (a) : (b))

// Internal function: called in taskbar_draw whenever the scale parameter changed from the last taskbar_draw call
// Set's the font size and regenerates the font bitmaps.
int taskbar_per_monitor_data_set_font_size(struct Taskbar *tb, int monitor_index, float scale) {
	if (tb == NULL) {
		printf("taskbar: taskbar_per_monitor_data_set_font_size called with a NULL pointer. Remember: your Taskbar struct should be on the stack!\n");
		assert(0);
	}

	struct TaskbarPerMonitorData *m = &tb->per_monitor_data[monitor_index];
	if (!m->is_initialized) {
		printf("taskbar: taskbar_per_monitor_data_set_font_size called on uninitialized per-monitor data\n");
		assert(0);
	}

	m->font_size = 22 * scale;

	FT_Error err = fontbmp_generate(&m->font, m->font_name, m->font_size);
	printf("Font bitmaps regenerated with size %i\n", m->font_size);
	if (err) {
		printf("fontbmp_generate returned an error (scale: %f, last_scale: %f)\n", scale, m->last_scale);
		return err;
	}

	return 0;
}

// Internal function: called in taskbar_draw per monitor if it hasn't already been initialized
int taskbar_per_monitor_data_initialize(struct Taskbar *tb, int monitor_index, float scale) {
	if (tb == NULL) {
		printf("taskbar: taskbar_per_monitor_data_initialize called with a NULL pointer. Remember: your Taskbar struct should be on the stack!\n");
		assert(0);
	}

	struct TaskbarPerMonitorData *m = &tb->per_monitor_data[monitor_index];
	if (m->is_initialized) {
		printf("taskbar: taskbar_per_monitor_data_initialize called on already initialized per-monitor data\n");
		assert(0);
	}

	m->is_initialized = 1;
	m->font_name = tb->filename_lekton_font;
	m->last_scale = scale;
	m->font = fontbmp_initialize();
	if (tb->debug) {
		m->debug_string = malloc(64);
	}

	int err = taskbar_per_monitor_data_set_font_size(tb, monitor_index, scale);
	assert(err == 0);

	return 0;
}

// Internal function: called in taskbar_deinitialize()
void taskbar_per_monitor_data_deinitialize(struct Taskbar *tb, int monitor_index) {
	if (tb == NULL) {
		printf("taskbar: taskbar_per_monitor_data_deinitialize called with a NULL pointer. Remember: your Taskbar struct should be on the stack!\n");
		assert(0);
	}

	struct TaskbarPerMonitorData *m = &tb->per_monitor_data[monitor_index];
	if (!m->is_initialized) {
		printf("taskbar: taskbar_per_monitor_data_deinitialize called on uninitialized per-monitor data\n");
		assert(0);
	}

	m->is_initialized = 0;
	fontbmp_deinitialize(m->font);
	free(m->debug_string);
}

bool taskbar_json_eq(sj_Value val, char *s) {
	size_t len = val.end - val.start;
	return strlen(s) == len && !memcmp(s, val.start, len);
}

// Returns non-zero on error
int taskbar_read_workspace_json(struct TaskbarWorkspace *workspace, sj_Reader *r, sj_Value root) {
	memset(workspace, 0, sizeof(struct TaskbarWorkspace));

	int hasNum = 0;

	sj_Value key, val;
	while (sj_iter_object(r, root, &key, &val)) {
		if (taskbar_json_eq(key, "urgent")) {
			workspace->urgent = val.start[0] == 't';
		} else if (taskbar_json_eq(key, "focused")) {
			workspace->focused = val.start[0] == 't';
		} else if (taskbar_json_eq(key, "visible")) {
			workspace->visible = val.start[0] == 't';
		} else if (taskbar_json_eq(key, "output")) {
			size_t len = val.end - val.start;
			workspace->output = malloc(len + 1); // free'd in taskbar_deinitialize()
			workspace->output[len] = 0;
			memcpy(workspace->output, val.start, len);
		} else if (taskbar_json_eq(key, "num")) {
			hasNum = 1;
			workspace->num = atoi(val.start);
			// Bounds check for our hardcoded length 10 workspaces array
			assert(workspace->num > 0);
			assert(workspace->num <= 10);
		}
	}

	return !hasNum;
}

// Modifies only taskbar.workspaces and taskbar.workspaces_mutex
void *taskbar_sway_ipc_thread(void *taskbar) {
	struct Taskbar *tb = taskbar;
	swayipc_initialize(&tb->ipc);

	int err = swayipc_connect(&tb->ipc);
	if (err) {
		return NULL;
	}

	// Set up workspaces with GET_WORKSPACES message
	int headerSize = 14; // 6 + 4 + 4
	sj_Reader r = sj_reader(tb->ipc.initialWorkspacesPacket + headerSize, tb->ipc.initialWorkspacesPacketSize - headerSize);
	sj_Value root = sj_read(&r);
	sj_Value element;
	while (sj_iter_array(&r, root, &element)) {
		struct TaskbarWorkspace workspace;
		int err = taskbar_read_workspace_json(&workspace, &r, element);
		workspace.exists = 1;

		if (!err) {
			pthread_mutex_lock(&tb->workspaces_mutex);
			tb->workspaces[workspace.num - 1] = workspace;
			pthread_mutex_unlock(&tb->workspaces_mutex);
		}
	}

	while (1) {
		char *packet;
		int packetSize;
		swayipc_receive_packet(&tb->ipc, &packet, &packetSize);

		if (tb->debug) {
			printf("PACKET: ");
			fflush(stdout);
			write(1, packet, packetSize);
			fflush(stdout);
			printf("\n");
			fflush(stdout);
		}

		// Parse the JSON body
		char change[7]; // "reload" (longest change string) + null byte

		sj_Reader r = sj_reader(packet + headerSize, packetSize - headerSize);
		sj_Value root = sj_read(&r);
		sj_Value key, val;

		int oldErr = 1, currentErr = 1;
		struct TaskbarWorkspace old, current;
		memset(&old, 0, sizeof(struct TaskbarWorkspace));
		memset(&current, 0, sizeof(struct TaskbarWorkspace));
		while (sj_iter_object(&r, root, &key, &val)) {
			size_t len = val.end - val.start;

			if (taskbar_json_eq(key, "change")) {
				assert(len <= 6);
				memcpy(change, val.start, len);
				change[len] = 0;
			} else if (taskbar_json_eq(key, "old")) {
				if (val.start[0] == 'n') { // "null"
					continue;
				}

				oldErr = taskbar_read_workspace_json(&old, &r, val);
			} else if (taskbar_json_eq(key, "current")) {
				if (val.start[0] == 'n') { // "null"
					continue;
				}

				currentErr = taskbar_read_workspace_json(&current, &r, val);
			}
		}

		int workspace_update = 0;
		if (memcmp(change, "empty\0", 6) == 0) {
			// Delete the workspace
			if (!currentErr) {
				workspace_update = 1;

				pthread_mutex_lock(&tb->workspaces_mutex);
				tb->workspaces[current.num - 1].exists = 0;
				pthread_mutex_unlock(&tb->workspaces_mutex);
			}
		} else if (memcmp(change, "init\0", 5) == 0) {
			// Create new workspace without necessarily focusing it
			if (!currentErr) {
				workspace_update = 1;

				pthread_mutex_lock(&tb->workspaces_mutex);
				tb->workspaces[current.num - 1] = current;
				tb->workspaces[current.num - 1].exists = 1;
				pthread_mutex_unlock(&tb->workspaces_mutex);
			}
		} else {
			// Old
			if (!oldErr) {
				workspace_update = 1;

				// Update the workspace
				pthread_mutex_lock(&tb->workspaces_mutex);
				tb->workspaces[old.num - 1].focused = 0;
				pthread_mutex_unlock(&tb->workspaces_mutex);
			}

			// Current
			if (!currentErr) {
				workspace_update = 1;

				current.exists = 1;
				current.focused = 1;
				// Update the workspace
				pthread_mutex_lock(&tb->workspaces_mutex);
				tb->workspaces[current.num - 1] = current;
				pthread_mutex_unlock(&tb->workspaces_mutex);
			}
		}

		if (workspace_update) {
			pthread_mutex_lock(&tb->need_handle_input_mutex);
			tb->need_handle_input = 1;
			pthread_mutex_unlock(&tb->need_handle_input_mutex);
		}
	}

	// Unreachable...
	swayipc_deinitialize(&tb->ipc);
	return NULL;
}

// Returns 0 on success, non-zero on error.
int taskbar_initialize(struct Taskbar *tb, char *assets_folder) {
	if (tb == NULL) {
		printf("taskbar: taskbar_initialize called with a NULL pointer. Remember: your Taskbar struct should be on the stack!\n");
		assert(0);
	}

	tb->hovered_workspace_index = -1;

	// Wasteful to do multiple allocations, but it is simple...
	tb->filename_lekton_font = malloc(256);
	sprintf(tb->filename_lekton_font, "%s/Lekton-Regular-Edited.ttf", assets_folder);

	tb->filename_background = malloc(256);
	sprintf(tb->filename_background, "%s/background.png", assets_folder);

	swr_initialize(&tb->swr);

	int channels;
	tb->background_bitmap = stbi_load(tb->filename_background, &tb->background_width, &tb->background_height, &channels, 4);
	swr_convert_image_abgr_to_argb((uint32_t*)tb->background_bitmap, tb->background_width * tb->background_height);

	memset(tb->per_monitor_data, 0, TASKBAR_MAX_MONITORS * sizeof(struct TaskbarPerMonitorData));

	pthread_mutex_init(&tb->need_handle_input_mutex, NULL);

	pthread_mutex_init(&tb->workspaces_mutex, NULL);
	pthread_t swayThread;
	pthread_create(&swayThread, NULL, taskbar_sway_ipc_thread, tb);

	return 0;
}

void taskbar_deinitialize(struct Taskbar *tb) {
	stbi_image_free(tb->background_bitmap);

	free(tb->filename_lekton_font);
	free(tb->filename_background);

	for (int i = 0; i < TASKBAR_MAX_MONITORS; i++) {
		if (tb->per_monitor_data[i].is_initialized) {
			taskbar_per_monitor_data_deinitialize(tb, i);
		}
	}

	pthread_mutex_lock(&tb->workspaces_mutex);
	for (int i = 0; i < 10; i++) {
		free(tb->workspaces[i].output);
	}
	pthread_mutex_unlock(&tb->workspaces_mutex);
}

// Remember to lock the workspace mutex before calling this function!
// Returns -1 if no workspace is hovered.
int taskbar_get_hovered_workspace(struct Taskbar *tb, char *monitor_name, int width, int height, int mouse_x, int mouse_y, int bar_height_at_1x_scale) {
	float scale = (float)height / (float)bar_height_at_1x_scale;
	int workspaceX = 0;
	int workspaceXStep = 30 * scale;
	int workspaceIndexHovered = -1;

	// Find the hovered workspace
	for (int i = 0; i < 10; i++) {
		if (tb->workspaces[i].exists == 0) {
			continue;
		}

		if (monitor_name != NULL && memcmp(tb->workspaces[i].output, monitor_name, strlen(monitor_name)) != 0) {
			continue;
		}

		struct Rect hitbox = {
			.x = workspaceX,
			.y = 0,
			.w = workspaceXStep,
			.h = height,
		};

		if (swr_is_point_in_rect(hitbox, mouse_x, mouse_y)) {
			workspaceIndexHovered = i;
			break;
		}

		workspaceX += workspaceXStep;
	}

	return workspaceIndexHovered;
}

void taskbar_handle_input_event(struct Taskbar *tb, int monitor_index, char *monitor_name, struct TaskbarEvent e, int width, int height, int bar_height_at_1x_scale) {
	if (tb == NULL) {
		return;
	}

	assert(monitor_index >= 0);
	assert(monitor_index < TASKBAR_MAX_MONITORS);

	e.monitor_index = monitor_index;
	tb->last_event = e;

	pthread_mutex_lock(&tb->workspaces_mutex);

	tb->hovered_workspace_index = taskbar_get_hovered_workspace(tb, monitor_name, width, height, e.mouse_x, e.mouse_y, bar_height_at_1x_scale);

	if (e.type == TB_MouseEnter) {
		tb->is_mouse_inside = 1;
	} else if (e.type == TB_MouseLeave) {
		tb->is_mouse_inside = 0;
		tb->hovered_workspace_index = -1;
	} else if (e.type == TB_MouseMoved) {
		if (tb->hovered_workspace_index == -1) {
			goto done;
		}
	} else if (e.type == TB_ScrollVertical) {
		int next = -1;
		int prev = -1;
		int foundCurrent = 0;
		for (int i = 0; i < 10; i++) {
			if (tb->workspaces[i].exists == 0) {
				continue;
			}

			if (monitor_name != NULL && memcmp(tb->workspaces[i].output, monitor_name, strlen(monitor_name)) != 0) {
				continue;
			}

			if (foundCurrent) {
				next = i;
				break;
			}

			if (tb->workspaces[i].focused) {
				foundCurrent = 1;
			}

			if (!foundCurrent) {
				prev = i;
			}
		}

		if (e.scroll_value > 0) { // Scroll down (right)
			if (next != -1) swayipc_switch_workspace(&tb->ipc, next + 1);
		} else if (e.scroll_value < 0) { // Scroll up (left)
			if (prev != -1) swayipc_switch_workspace(&tb->ipc, prev + 1);
		}

	} else if (e.type == TB_Mouse1Pressed) {
		if (tb->hovered_workspace_index == -1) {
			goto done;
		}

		swayipc_switch_workspace(&tb->ipc, tb->hovered_workspace_index+1);
	}

done:
	pthread_mutex_unlock(&tb->workspaces_mutex);
}

void taskbar_draw(struct Taskbar *tb, int monitor_index, char *monitor_name, uint32_t *framebuffer, int width, int height, int bar_height_at_1x_scale) {
	if (tb == NULL) {
		return;
	}

	assert(monitor_index >= 0);
	assert(monitor_index < TASKBAR_MAX_MONITORS);

	if (monitor_index == tb->last_event.monitor_index) {
		pthread_mutex_lock(&tb->need_handle_input_mutex);
		if (tb->is_mouse_inside && tb->need_handle_input) {
			tb->need_handle_input = 0;
			struct TaskbarEvent e = tb->last_event;
			e.type = TB_MouseMoved;
			taskbar_handle_input_event(tb, monitor_index, monitor_name, e, width, height, bar_height_at_1x_scale);
		}
		pthread_mutex_unlock(&tb->need_handle_input_mutex);
	}

	struct timespec startTime;
	if (tb->debug) {
		clock_gettime(CLOCK_MONOTONIC, &startTime);
	}

	float scale = (float)height / (float)bar_height_at_1x_scale;

	struct TaskbarPerMonitorData *m = &tb->per_monitor_data[monitor_index];
	if (!m->is_initialized) {
		taskbar_per_monitor_data_initialize(tb, monitor_index, scale);
	}

	m->frame_number += 1;

	if (m->frame_number % 30 == 0) {
		char clock[8+1];
		taskbar_clock_string(clock);
		if (clock[7] == '0' || clock[7] == '5') {
			memcpy(tb->clock, clock, 8+1);
			m->max_render_time_last_5s = 0.0;
		}
	}

	// Set the font size if scale changed
	if (scale != m->last_scale) {
		int err = taskbar_per_monitor_data_set_font_size(tb, monitor_index, scale);
		assert(err == 0);
	}

	swr_set_output(&tb->swr, framebuffer, width, height);

	// Draw background with its own scale
	float background_scale = MAX((float)width / tb->background_width, (float)height / tb->background_height);
	//swr_draw_image_ex(&tb->swr, (uint32_t*)tb->background_bitmap, tb->background_width, tb->background_height, 0xFFFFFFFF, background_scale, 0, 0);
	swr_draw_fill_background(&tb->swr, 0, 0, 0);

	// Draw upper highlight rectangle
	int highlightHeight = MAX(1.0, floor(2*background_scale));
	int highlightAlpha = 50;
	struct Rect rect = {
		.x = 0,
		.y = 0,
		.w = width,
		.h = highlightHeight,
	};
	swr_draw_rectangle(&tb->swr, rect, swr_rgba(255,255,255,highlightAlpha));

	// Draw clock on the right
	swr_draw_text_ex(&tb->swr, tb->clock, &m->font, swr_rgb(0,0,0), width - 97 * scale, 6 * scale);
	swr_draw_text_ex(&tb->swr, tb->clock, &m->font, TEXT_COLOR, width - 97 * scale, 6 * scale);

	// Draw workspaces on the left
	pthread_mutex_lock(&tb->workspaces_mutex);
	char s[3]; // Enough for "10\0"
	int workspaceXStep = 30 * scale;
	int workspaceSize = 23 * scale;
	int workspaceX = ceil((((float)height - (float)highlightHeight) - (float)workspaceSize) / 2.0);
	int workspaceXInitial = workspaceX;
	for (int i = 0; i < 10; i++) {
		if (tb->workspaces[i].exists == 0) {
			continue;
		}

		if (monitor_name != NULL && memcmp(tb->workspaces[i].output, monitor_name, strlen(monitor_name)) != 0) {
			continue;
		}

		sprintf(s, "%d", i+1);
		uint32_t textColor = TEXT_COLOR;

		struct Rect rect = {
			.x = workspaceX,
			.y = ceil(highlightHeight + (((float)height - (float)highlightHeight) - (float)workspaceSize) / 2.0),
			.w = workspaceSize,
			.h = workspaceSize,
		};

		// Draw focus outline when workspace is focused
		if (tb->workspaces[i].focused) {
			float radius = 4.0 * scale;
			float grow = (scale - 1.0) / 2.0;
			// We want to grow inner and outer by an integer amount to avoid blurry output
			float growInner = (int)(grow+0.5);
			float growOuter = 1.0 + (int)grow;
			if (scale < 1.0) {
				growInner = 0.0;
				growOuter = 0.0;
			}

			// Inner
			swr_draw_rectangle_rounded(&tb->swr, rect, 0x3063eb72, radius);
			// Outline
			swr_draw_rectangle_rounded_outline(&tb->swr, rect, 0xFF63eb72, radius, growInner, growOuter);
		}

		// Draw hovered rectangle when workspace is hovered
		if (tb->hovered_workspace_index == i) {
			struct Rect hoveredRect = {
				.x = workspaceX - workspaceXInitial,
				.y = highlightHeight,
				.w = workspaceXStep,
				.h = height - highlightHeight,
			};
			swr_draw_rectangle(&tb->swr, hoveredRect, swr_rgba(255,255,255,highlightAlpha));
		}

		// Draw workspace number
		struct Rect glyphBbox = swr_measure_text_ex(&tb->swr, s, &m->font, textColor, 0, 0);
		int xPos = ceil((float)rect.x + (float)rect.w / 2.0 - (float)glyphBbox.w / 2.0 - glyphBbox.x);
		int yPos = (float)rect.y + (float)rect.h / 2.0 - (float)glyphBbox.h / 2.0 - glyphBbox.y;
		swr_draw_text_ex(&tb->swr, s, &m->font, swr_rgba(0,0,0,100), xPos+1, yPos+1); // DROPSHADOW
		swr_draw_text_ex(&tb->swr, s, &m->font, textColor, xPos, yPos);

		workspaceX += workspaceXStep;
	}
	pthread_mutex_unlock(&tb->workspaces_mutex);

	// Draw debug info
	if (tb->debug) {
		struct timespec endTime;
		clock_gettime(CLOCK_MONOTONIC, &endTime);
		double renderTimeMs = 1000 * ((endTime.tv_sec - startTime.tv_sec) + (endTime.tv_nsec - startTime.tv_nsec) * 1e-9);
		m->max_render_time_last_5s = MAX(renderTimeMs, m->max_render_time_last_5s);
		sprintf(m->debug_string, "render: %.3fms (5s max: %.3fms)", renderTimeMs, m->max_render_time_last_5s);

		struct Rect bounds = swr_measure_text_ex(&tb->swr, m->debug_string, &m->font, swr_rgb(255,255,255), 0, 0);
		swr_draw_text_ex(&tb->swr, m->debug_string, &m->font, swr_rgba(255,255,255,30), (float)width / 2.0 - (float)bounds.w / 2.0, 6*scale);
	}

	m->last_scale = scale;
}

// s needs to be atleast 8 bytes
void taskbar_clock_string(char *s) {
	struct timeval tv;
	struct timezone tz;
	int error = gettimeofday(&tv, &tz);
	if (error) {
		return;
	}

	struct tm *today = localtime(&tv.tv_sec);
	sprintf(s, "%02d:%02d:%02d", today->tm_hour, today->tm_min, today->tm_sec);
}
