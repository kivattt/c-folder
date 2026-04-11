#pragma once

#include <stdint.h>
#include <stdlib.h>
#include "../sw-render/sw-render.h"

struct Taskbar {
	// TODO
};

enum TaskbarEventType {
	TB_None = 0,
	TB_MouseButton = 1,
	TB_MouseMoved = 2,
};

struct TaskbarEvent {
	enum TaskbarEventType type;
	int mouse_x;
	int mouse_y;
};

struct Taskbar *taskbar_initialize();
void taskbar_deinitialize(struct Taskbar *tb);
void taskbar_handle_input_event(struct Taskbar *tb, int monitor_index, struct TaskbarEvent e);
void taskbar_draw(struct Taskbar *tb, int monitor_index, uint32_t *framebuffer, int width, int height);
