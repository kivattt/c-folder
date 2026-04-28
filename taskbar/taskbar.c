#include "taskbar.h"
#include <bits/time.h>

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
}

bool eq(sj_Value val, char *s) {
	size_t len = val.end - val.start;
	return strlen(s) == len && !memcmp(s, val.start, len);
}

// Returns non-zero on error
int read_workspace_json(struct TaskbarWorkspace *workspace, sj_Reader *r, sj_Value root) {
	memset(workspace, 0, sizeof(struct TaskbarWorkspace));

	int hasNum = 0;

	sj_Value key, val;
	while (sj_iter_object(r, root, &key, &val)) {
		if (eq(key, "urgent")) {
			workspace->urgent = val.start[0] == 't';
		} else if (eq(key, "focused")) {
			workspace->focused = val.start[0] == 't';
		} else if (eq(key, "visible")) {
			workspace->visible = val.start[0] == 't';
		} else if (eq(key, "output")) {
			size_t len = val.end - val.start;
			workspace->output = malloc(len + 1);
			workspace->output[len] = 0;
			memcpy(workspace->output, val.start, len);
		} else if (eq(key, "num")) {
			hasNum = 1;
			workspace->num = atoi(val.start);
			// Bounds check for our hardcoded length 10 workspaces array
			assert(workspace->num > 0);
			assert(workspace->num <= 10);
		}
	}

	return !hasNum;
}

// TEMPORARY: REMOVE THIS REMOVE THIS REMOVE THIS
void print_workspace(struct TaskbarWorkspace workspace) {
	printf("\tNum: %i\n", workspace.num);
	printf("\tUrgent: %i\n", workspace.urgent);
	printf("\tFocused: %i\n", workspace.focused);
	printf("\tVisible: %i\n", workspace.visible);
	if (workspace.output == NULL) {
		printf("\tOutput: NULL\n");
	} else {
		printf("Output: %s\n", workspace.output);
	}
}

// Modifies only taskbar.workspaces and taskbar.workspaces_mutex
void *sway_ipc_thread(void *taskbar) {
	struct Taskbar *tb = taskbar;

	struct SwayIPC ipc;
	swayipc_initialize(&ipc);

	int err = swayipc_connect(&ipc);
	if (err) {
		return NULL;
	}

	// Set up workspaces with GET_WORKSPACES message
	int headerSize = 14; // 6 + 4 + 4
	sj_Reader r = sj_reader(ipc.initialWorkspacesPacket + headerSize, ipc.initialWorkspacesPacketSize - headerSize);
	sj_Value root = sj_read(&r);
	sj_Value element;
	while (sj_iter_array(&r, root, &element)) {
		struct TaskbarWorkspace workspace;
		int err = read_workspace_json(&workspace, &r, element);

		if (!err) {
			print_workspace(workspace);

			pthread_mutex_lock(&tb->workspaces_mutex);
			tb->workspaces[workspace.num - 1] = workspace;
			pthread_mutex_unlock(&tb->workspaces_mutex);
		}
	}

	while (1) {
		char *packet;
		int packetSize;
		swayipc_receive_packet(&ipc, &packet, &packetSize);

		printf("PACKET: ");
		fflush(stdout);
		write(1, packet, packetSize);
		printf("\n");

		// Parse the JSON body
		char change[7]; // "reload" (longest change string) + null byte

		sj_Reader r = sj_reader(packet + headerSize, packetSize - headerSize);
		sj_Value root = sj_read(&r);
		sj_Value key, val;
		while (sj_iter_object(&r, root, &key, &val)) {
			size_t len = val.end - val.start;

			if (eq(key, "change")) {
				assert(len <= 6);
				memcpy(change, val.start, len);
				change[len] = 0;
			} else if (eq(key, "old")) {
				struct TaskbarWorkspace workspace;
				int err = read_workspace_json(&workspace, &r, val);

				if (!err) {
					// Update the workspace
					pthread_mutex_lock(&tb->workspaces_mutex);
					tb->workspaces[workspace.num - 1].focused = 0;
					pthread_mutex_unlock(&tb->workspaces_mutex);
				}
			} else if (eq(key, "current")) {
				struct TaskbarWorkspace workspace;
				int err = read_workspace_json(&workspace, &r, val);

				if (!err) {
					workspace.focused = 1;
					// Update the workspace
					pthread_mutex_lock(&tb->workspaces_mutex);
					tb->workspaces[workspace.num - 1] = workspace;
					pthread_mutex_unlock(&tb->workspaces_mutex);
				}
			}
		}
	}

	// Unreachable...
	swayipc_deinitialize(&ipc);
	return NULL;
}

// Returns 0 on success, non-zero on error.
int taskbar_initialize(struct Taskbar *tb, char *assets_folder) {
	if (tb == NULL) {
		printf("taskbar: taskbar_initialize called with a NULL pointer. Remember: your Taskbar struct should be on the stack!\n");
		assert(0);
	}

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

	pthread_mutex_init(&tb->workspaces_mutex, NULL);
	pthread_t swayThread;
	pthread_create(&swayThread, NULL, sway_ipc_thread, tb);

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

void taskbar_handle_input_event(struct Taskbar *tb, int monitor_index, struct TaskbarEvent e) {
	tb->last_event = e;

	/*printf("input on monitor %i (type: %i):\n", monitor_index, e.type);
	printf("\tmouse_x = %i\n", e.mouse_x);
	printf("\tmouse_y = %i\n", e.mouse_y);*/
}

void taskbar_draw(struct Taskbar *tb, int monitor_index, uint32_t *framebuffer, int width, int height, float scale /* unused */, int bar_height_at_1x_scale) {
	if (tb == NULL) {
		return;
	}

	struct timespec startTime;
	int startTimeRes = clock_gettime(CLOCK_MONOTONIC, &startTime);
	if (startTimeRes != 0) {
		printf("clock_gettime() returned an error\n");
	}

	assert(monitor_index >= 0);
	assert(monitor_index < TASKBAR_MAX_MONITORS);

	scale = (float)height / tb->background_height;

	struct TaskbarPerMonitorData *m = &tb->per_monitor_data[monitor_index];
	if (!m->is_initialized) {
		taskbar_per_monitor_data_initialize(tb, monitor_index, scale);
	}

	m->frame_number += 1;

	if (m->frame_number % 30 == 0) {
		char clock[8+1];
		clock_string(clock);
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

	// Draw the background with its own scale
	float background_scale = MAX((float)width / tb->background_width, (float)height / tb->background_height);
	swr_draw_image_ex(&tb->swr, (uint32_t*)tb->background_bitmap, tb->background_width, tb->background_height, 0xFFFFFFFF, background_scale, 0, 0);

	swr_draw_text_ex(&tb->swr, tb->clock, &m->font, swr_rgb(0,0,0), width - 97 * scale, 6 * scale);
	swr_draw_text_ex(&tb->swr, tb->clock, &m->font, TEXT_COLOR, width - 97 * scale, 6 * scale);

	pthread_mutex_lock(&tb->workspaces_mutex);
	char *s = malloc(32); // DEBUGGING
	for (int i = 0; i < 10; i++) {
		if (tb->workspaces[i].output == NULL) continue; // FIXME

		sprintf(s, "%d", i+1);
		uint32_t color = swr_rgb(255, 255, 255);
		if (tb->workspaces[i].focused) {
			color = swr_rgb(0, 255, 255);
		}

		swr_draw_text(&tb->swr, s, 22*scale, color, i*40, 0);
	}
	pthread_mutex_unlock(&tb->workspaces_mutex);

	struct timespec endTime;
	int endTimeRes = clock_gettime(CLOCK_MONOTONIC, &endTime);
	if (endTimeRes != 0) {
		printf("clock_gettime() returned an error\n");
	}

	double renderTimeMs = 1000 * ((endTime.tv_sec - startTime.tv_sec) + (endTime.tv_nsec - startTime.tv_nsec) * 1e-9);
	m->max_render_time_last_5s = MAX(renderTimeMs, m->max_render_time_last_5s);
	sprintf(s, "render: %.3fms (5s max: %.3fms)", renderTimeMs, m->max_render_time_last_5s);
	swr_draw_text(&tb->swr, s, 22*scale, swr_rgb(255,255,255), 100, 0);

	free(s); // DEBUGGING

	m->last_scale = scale;
}

// s needs to be atleast 8 bytes
void clock_string(char *s) {
	struct timeval tv;
	struct timezone tz;
	int error = gettimeofday(&tv, &tz);
	if (error) {
		return;
	}

	struct tm *today = localtime(&tv.tv_sec);
	sprintf(s, "%02d:%02d:%02d", today->tm_hour, today->tm_min, today->tm_sec);
}
