#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#include "../sw-render/sw-render.h"
#include "../swayipc/swayipc.h"
#include "sj.h"

// Max amount of taskbars (screens, workspaces).
#define TASKBAR_MAX_MONITORS 10
#define TASKBAR_MAX_SWAYBG_CMDLINE_OUTPUT_ARGS 32

enum TaskbarEventType {
	TB_None = 0,
	TB_MouseMoved = 1,

	TB_Mouse1Pressed = 2,
	TB_Mouse1Released = 3,

	TB_Mouse2Pressed = 4,
	TB_Mouse2Released = 5,

	TB_ScrollVertical = 6, // See: TaskbarEvent->scroll_value
	TB_MouseEnter = 7,
	TB_MouseLeave = 8,
};

enum TaskbarSwayBGCmdlineParseState {
	TASKBAR_SWAYBG_CMDLINE_PARSE_STATE_FLAG = 0,
	TASKBAR_SWAYBG_CMDLINE_PARSE_STATE_OUTPUT = 1,
	TASKBAR_SWAYBG_CMDLINE_PARSE_STATE_IMAGE = 2,
	TASKBAR_SWAYBG_CMDLINE_PARSE_STATE_MODE = 3,
};

struct TaskbarEvent {
	enum TaskbarEventType type;
	int monitor_index;
	int mouse_x;
	int mouse_y;
	double scroll_value;
};

// Per-monitor data (assumed to stay at the same index per monitor name..)
struct TaskbarPerMonitorData {
	int is_initialized;
	unsigned long long frame_number;

	unsigned char *background_bitmap;
	int background_width;
	int background_height;

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
	char *output; // Monitor name (allocated in read_workspace_json)
};

// Command-line arguments passed to currently running swaybg process
struct TaskbarSwayBGCmdline {
	char *output; // Monitor name (-o, --output)
	char *image_path; // Background image file path (-i, --image)
	char *scaling_mode; // Scale mode (-m, --mode)
};

struct Taskbar {
	int debug; // Set to 1 to enable debug stuff

	// Global data
	struct SWRender swr;
	struct SwayIPC ipc;
	char clock[8+1]; // Enough for "01:23:45" (including the null byte)
	char date_human[20]; // Enough for "Fri May 22" (including the null byte)
	char date_numbers[20]; // Enough for "2026-05-22" (including the null byte)
	char ram_usage[64]; // Enough for "RAM: 16.0 GB/16.0 GB"
	char battery_percentage[20]; // Enough for "100.0%" (including the null byte)
	char *filename_lekton_font;
	char *filename_background;
	int hovered_workspace_index;

	struct TaskbarEvent last_event;
	int is_mouse_inside;
	int need_handle_input; // When a workspace changed, the hovered one may have changed even when no new mouse inputs were handled. Forces a TB_MouseMoved event in this case.
	pthread_mutex_t need_handle_input_mutex;

	struct TaskbarWorkspace workspaces[10];
	pthread_mutex_t workspaces_mutex;

	struct TaskbarSwayBGCmdline swaybg_cmdline[TASKBAR_MAX_SWAYBG_CMDLINE_OUTPUT_ARGS];

	// Per-monitor data
	struct TaskbarPerMonitorData per_monitor_data[TASKBAR_MAX_MONITORS];
};

int taskbar_initialize(struct Taskbar *tb, char *assets_folder);
void taskbar_deinitialize(struct Taskbar *tb);
void taskbar_handle_input_event(struct Taskbar *tb, int monitor_index, char *monitor_name, struct TaskbarEvent e, int width, int height, int bar_height_at_1x_scale);
void taskbar_draw(struct Taskbar *tb, int monitor_index, char *monitor_name, uint32_t *framebuffer, int width, int height, int bar_height_at_1x_scale);

// Internal functions
void taskbar_clock_string(char *s);
int taskbar_per_monitor_data_initialize(struct Taskbar *tb, int monitor_index, float scale);
void taskbar_per_monitor_data_deinitialize(struct Taskbar *tb, int monitor_index);
int taskbar_per_monitor_data_set_font_size(struct Taskbar *tb, int monitor_index, float scale);
void *taskbar_sway_ipc_thread(void *taskbar); // Modifies only taskbar.workspaces and taskbar.workspaces_mutex
bool taskbar_json_eq(sj_Value, char *s);
int taskbar_read_workspace_json(struct TaskbarWorkspace *workspace, sj_Reader *r, sj_Value root);
int taskbar_get_hovered_workspace(struct Taskbar *tb, char *monitor_name, int width, int height, int mouse_x, int mouse_y, int bar_height_at_1x_scale);
void taskbar_date_human_string(char *s);
void taskbar_date_numbers_string(char *s);
float taskbar_get_battery_percentage();
void taskbar_ram_usage_string(char *s);
