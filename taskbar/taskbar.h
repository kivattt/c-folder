#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include "../sw-render/sw-render.h"

#include "stb_image.h"

enum TaskbarEventType {
	TB_None = 0,
	TB_MouseMoved = 1,

	TB_Mouse1Pressed = 2,
	TB_Mouse1Released = 3,

	TB_Mouse2Pressed = 4,
	TB_Mouse2Released = 5,
};

struct TaskbarEvent {
	enum TaskbarEventType type;
	int mouse_x;
	int mouse_y;
};

struct Taskbar {
	float last_scale;
	struct TaskbarEvent last_event;
	struct Font font;
	char *font_name;
	int font_size;
	char clock[8+1];

	unsigned char *background_bitmap;
	int background_width;
	int background_height;
};

struct Taskbar *taskbar_initialize();
void taskbar_deinitialize(struct Taskbar *tb);
void taskbar_handle_input_event(struct Taskbar *tb, int monitor_index, struct TaskbarEvent e);
void taskbar_draw(struct Taskbar *tb, int monitor_index, uint32_t *framebuffer, int width, int height, float scale);

// Internal functions

