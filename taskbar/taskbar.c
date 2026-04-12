#include "taskbar.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define BACKGROUND_COLOR swr_color_to_argb(42, 44, 46)
#define TEXT_COLOR swr_color_to_argb(220, 220, 220)

struct Taskbar *taskbar_initialize() {
	struct Taskbar *tb = malloc(sizeof(struct Taskbar));
	tb->last_scale = 1.0;

	tb->font_size = 22;
	tb->font_name = "assets/Inconsolata-Regular.ttf";

	tb->font = fontbmp_initialize();
	fontbmp_generate(&tb->font, tb->font_name, 24);

	int channels;
	tb->background_bitmap = stbi_load("assets/background.png", &tb->background_width, &tb->background_height, &channels, 4);
	swr_convert_image_abgr_to_argb((uint32_t*)tb->background_bitmap, tb->background_width * tb->background_height);
	return tb;
}

void taskbar_deinitialize(struct Taskbar *tb) {
	fontbmp_deinitialize(tb->font);
	stbi_image_free(tb->background_bitmap);

	free(tb);
}

void taskbar_handle_input_event(struct Taskbar *tb, int monitor_index, struct TaskbarEvent e) {
	tb->last_event = e;

	printf("input on monitor %i (type: %i):\n", monitor_index, e.type);
	printf("\tmouse_x = %i\n", e.mouse_x);
	printf("\tmouse_y = %i\n", e.mouse_y);
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

void taskbar_draw(struct Taskbar *tb, int monitor_index, uint32_t *framebuffer, int width, int height, float scale) {
	if (tb == NULL) {
		return;
	}

	clock_string(tb->clock);

	// Set the font size if scale changed
	if (scale != tb->last_scale) {
		fontbmp_generate(&tb->font, tb->font_name, tb->font_size * scale);
	}

	for (int i = 0; i < width * height; i++) {
		framebuffer[i] = BACKGROUND_COLOR;
	}

	swr_draw_image_argb(framebuffer, width, height, (uint32_t*)tb->background_bitmap, tb->background_width, tb->background_height, 0, 0);
	swr_draw_text(framebuffer, width, height, tb->clock, &tb->font, TEXT_COLOR, width - 97 * scale, (tb->font_size + 1) * scale);

	tb->last_scale = scale;
}
