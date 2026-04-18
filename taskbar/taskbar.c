#include "taskbar.h"

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
	if (err) {
		printf("fontbmp_generate returned an error (scale: %f, last_scale: %f)\n", scale, m->last_scale);
		return err;
	}

	return 0;
}

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

// Returns 0 on success, non-zero on error.
int taskbar_initialize(struct Taskbar *tb, char *assets_folder) {
	if (tb == NULL) {
		printf("taskbar: taskbar_initialize called with a NULL pointer. Remember: your Taskbar struct should be on the stack!\n");
		assert(0);
	}

	// Wasteful to do multiple allocations, but it is simple...
	tb->filename_lekton_font = malloc(256);
	sprintf(tb->filename_lekton_font, "%s/Lekton-Regular.ttf", assets_folder);

	tb->filename_background = malloc(256);
	sprintf(tb->filename_background, "%s/background.png", assets_folder);

	swr_initialize(&tb->swr);

	int channels;
	tb->background_bitmap = stbi_load(tb->filename_background, &tb->background_width, &tb->background_height, &channels, 4);
	swr_convert_image_abgr_to_argb((uint32_t*)tb->background_bitmap, tb->background_width * tb->background_height);

	memset(tb->per_monitor_data, 0, TASKBAR_MAX_MONITORS * sizeof(struct TaskbarPerMonitorData));

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
}

void taskbar_handle_input_event(struct Taskbar *tb, int monitor_index, struct TaskbarEvent e) {
	tb->last_event = e;

	printf("input on monitor %i (type: %i):\n", monitor_index, e.type);
	printf("\tmouse_x = %i\n", e.mouse_x);
	printf("\tmouse_y = %i\n", e.mouse_y);
}

void taskbar_draw(struct Taskbar *tb, int monitor_index, uint32_t *framebuffer, int width, int height, float scale, int bar_height_at_1x_scale) {
	if (tb == NULL) {
		return;
	}

	assert(monitor_index >= 0);
	assert(monitor_index < TASKBAR_MAX_MONITORS);

	struct TaskbarPerMonitorData *m = &tb->per_monitor_data[monitor_index];
	if (!m->is_initialized) {
		taskbar_per_monitor_data_initialize(tb, monitor_index, scale);
	}

	m->frame_number += 1;

	/*if (monitor_index == 1) {
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		long milliseconds = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
		printf("Milliseconds since epoch: %ld\n", milliseconds);
	}*/

	if (m->frame_number % 30 == 0) {
		clock_string(tb->clock);
	}

	// Set the font size if scale changed
	if (scale != m->last_scale) {
		int err = taskbar_per_monitor_data_set_font_size(tb, monitor_index, scale);
		assert(err == 0);
	}

	for (int i = 0; i < width * height; i++) {
		framebuffer[i] = BACKGROUND_COLOR;
	}

	swr_set_output(&tb->swr, framebuffer, width, height);
	float max_scale = MAX((width / 1920.0f), (height / (float)bar_height_at_1x_scale));
	swr_draw_image_ex(&tb->swr, (uint32_t*)tb->background_bitmap, tb->background_width, tb->background_height, 0xFFFFFFFF, max_scale, 0, 0);
	swr_draw_text_ex(&tb->swr, tb->clock, &m->font, swr_rgb(0,0,0), width - 97 * scale, 6 * scale);
	swr_draw_text_ex(&tb->swr, tb->clock, &m->font, TEXT_COLOR, width - 97 * scale, 6 * scale);

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
