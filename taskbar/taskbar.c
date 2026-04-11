#include "taskbar.h"

struct Taskbar *taskbar_initialize() {
	struct Taskbar *tb = malloc(sizeof(struct Taskbar));
	return tb;
}

void taskbar_deinitialize(struct Taskbar *tb) {
	free(tb);
}

void taskbar_handle_input_event(struct Taskbar *tb, int monitor_index, struct TaskbarEvent e) {
	printf("input on monitor %i (type: %i):\n", monitor_index, e.type);
	printf("\tmouse_x = %i\n", e.mouse_x);
	printf("\tmouse_y = %i\n", e.mouse_y);
}

void taskbar_draw(struct Taskbar *tb, int monitor_index, uint32_t *framebuffer, int width, int height, float scale) {
	for (int i = 0; i < width * height; i++) {
		framebuffer[i] = swr_color_to_argb(42, 44, 46);
	}

	// Thin highlight at the top row
	struct Rect rect = (struct Rect){
		.x = 0,
		.y = 0,
		.w = width,
		.h = 1,
	};
	swr_draw_rectangle_rounded(framebuffer, width, height, rect, 0x20FFFFFF, 0.0);

	for (int i = 0; i < 9; i++) {
		int w = 23;
		int leap = 6;
		struct Rect round_rect = (struct Rect){
			.x = 3 + i * (w + leap),
			.y = 4,
			.w = w,
			.h = w,
		};
		uint8_t red = 70 + i * 13;
		uint8_t green = 100 + i * 10;
		uint8_t blue = 255 - i * 5;
		swr_draw_rectangle_rounded_outline(framebuffer, width, height, round_rect, 0xFF000000 | red << 16 | green << 8 | blue, 8.0, 1.0 /*in*/, 1.0 /*out*/);
	}
}
