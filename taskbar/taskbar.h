#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include "../sw-render/sw-render.h"
#include "../swayipc/swayipc.h"
#include "sj.h"

// Max amount of taskbars (screens)
#define TASKBAR_MAX_MONITORS 64

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

// Per-monitor data
struct TaskbarPerMonitorData {
	int is_initialized;
	unsigned long long frame_number;
	char *monitor_name; // FIXME: Use this

	float last_scale;
	struct FontBMPFont font;
	char *font_name;
	int font_size;
	double max_render_time_last_5s;
	char *debug_string; // Allocated in taskbar_per_monitor_data_initialize()
};

struct TaskbarWorkspace {
	int exists;
	int num;
	int urgent;
	int focused;
	int visible; // ?
	char *output; // Allocated in read_workspace_json
};

struct Taskbar {
	int debug; // Set to 1 to enable debug stuff

	// Global data
	struct SWRender swr;
	struct TaskbarEvent last_event;
	char clock[8+1]; // Enough for "01:23:45" (including the null byte)
	char *filename_lekton_font;
	char *filename_background;

	unsigned char *background_bitmap;
	int background_width;
	int background_height;

	struct TaskbarWorkspace workspaces[10];
	pthread_mutex_t workspaces_mutex;

	// Per-monitor data
	struct TaskbarPerMonitorData per_monitor_data[TASKBAR_MAX_MONITORS];
};

int taskbar_initialize(struct Taskbar *tb, char *assets_folder);
void taskbar_deinitialize(struct Taskbar *tb);
void taskbar_handle_input_event(struct Taskbar *tb, int monitor_index, struct TaskbarEvent e);
void taskbar_draw(struct Taskbar *tb, int monitor_index, uint32_t *framebuffer, int width, int height, float scale, int bar_height_at_1x_scale);

// Internal functions
void clock_string(char *s);
int taskbar_per_monitor_data_initialize(struct Taskbar *tb, int monitor_index, float scale);
void taskbar_per_monitor_data_deinitialize(struct Taskbar *tb, int monitor_index);
int taskbar_per_monitor_data_set_font_size(struct Taskbar *tb, int monitor_index, float scale);
void *sway_ipc_thread(void *taskbar); // Modifies only taskbar.workspaces and taskbar.workspaces_mutex
bool eq(sj_Value, char *s);
int read_workspace_json(struct TaskbarWorkspace *workspace, sj_Reader *r, sj_Value root);
