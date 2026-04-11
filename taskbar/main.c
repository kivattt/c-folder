#define _GNU_SOURCE
#include <wayland-client.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <linux/input.h>

#include "protocols/wlr-layer-shell-client-protocol.h"
#include "taskbar.h"

#define BAR_HEIGHT 30

struct Buffer {
    struct wl_buffer *wl_buf;
    uint32_t *data;
    int busy;
};

struct BarMonitor {
    struct wl_output *output;
    struct wl_surface *surface;
    struct zwlr_layer_surface_v1 *layer_surface;

    struct Buffer buffers[2];
    int current;

    int width, height;      // surface coords
    int scale;              // integer scale (1,2,3...)
    int configured;

    struct wl_callback *frame_cb;
};

struct wl_display *display;
struct wl_registry *registry;
struct wl_compositor *compositor;
struct wl_shm *shm;
struct zwlr_layer_shell_v1 *layer_shell;

struct wl_seat *seat;
struct wl_pointer *pointer = NULL;

struct BarMonitor *bars = NULL;
int n_monitors = 0;

struct Taskbar *taskbar;

static struct wl_surface *current_surface = NULL;
static int current_x = 0;
static int current_y = 0;

/* ================= SHM ================= */
static int create_shm_file(size_t size) {
    char name[] = "/tmp/taskbar-XXXXXX";
    int fd = mkstemp(name);
    unlink(name);
    ftruncate(fd, size);
    return fd;
}

static void buffer_release(void *data, struct wl_buffer *wl_buf) {
    struct Buffer *buf = data;
    buf->busy = 0;
}

static const struct wl_buffer_listener buffer_listener = {
    .release = buffer_release,
};

static void create_buffer(struct Buffer *buf, int width, int height) {
    int stride = width * 4;
    int size = stride * height;

    int fd = create_shm_file(size);

    buf->data = mmap(NULL, size,
        PROT_READ | PROT_WRITE,
        MAP_SHARED, fd, 0);

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);

    buf->wl_buf = wl_shm_pool_create_buffer(
        pool, 0,
        width, height,
        stride,
        WL_SHM_FORMAT_ARGB8888
    );

    wl_shm_pool_destroy(pool);
    close(fd);

    buf->busy = 0;
    wl_buffer_add_listener(buf->wl_buf, &buffer_listener, buf);
}

/* ================= OUTPUT (SCALE) ================= */

static void output_scale(void *data, struct wl_output *output, int32_t scale) {
    struct BarMonitor *b = data;
    if (scale < 1) scale = 1;
    b->scale = scale;
}

static void output_geometry(void *data, struct wl_output *o,
    int32_t x, int32_t y, int32_t pw, int32_t ph,
    int32_t subpixel, const char *make,
    const char *model, int32_t transform) {}

static void output_mode(void *data, struct wl_output *o,
    uint32_t flags, int32_t w, int32_t h, int32_t refresh) {}

static void output_done(void *data, struct wl_output *o) {}

static const struct wl_output_listener output_listener = {
    .geometry = output_geometry,
    .mode = output_mode,
    .done = output_done,
    .scale = output_scale,
};

/* ================= INPUT ================= */

static struct BarMonitor *find_bar(struct wl_surface *s) {
    for (int i = 0; i < n_monitors; i++) {
        if (bars[i].surface == s)
            return &bars[i];
    }
    return NULL;
}

static void pointer_enter(void *data, struct wl_pointer *p,
    uint32_t serial, struct wl_surface *surface,
    wl_fixed_t sx, wl_fixed_t sy)
{
    current_surface = surface;
    current_x = wl_fixed_to_int(sx);
    current_y = wl_fixed_to_int(sy);
}

static void pointer_leave(void *data, struct wl_pointer *p,
    uint32_t serial, struct wl_surface *surface)
{
    current_surface = NULL;
}

static void pointer_motion(void *data, struct wl_pointer *p,
    uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
    if (!current_surface) return;

    struct BarMonitor *b = find_bar(current_surface);
    if (!b) return;

    current_x = wl_fixed_to_int(sx);
    current_y = wl_fixed_to_int(sy);

    int index = b - bars;

    struct TaskbarEvent e = {
        .type = TB_MouseMoved,
        .mouse_x = current_x,
        .mouse_y = current_y,
    };

    taskbar_handle_input_event(taskbar, index, e);
}

static void pointer_button(void *data, struct wl_pointer *p,
    uint32_t serial, uint32_t time,
    uint32_t button, uint32_t state)
{
    if (!current_surface) return;

    struct BarMonitor *b = find_bar(current_surface);
    if (!b) return;

    int index = b - bars;

	enum TaskbarEventType type = TB_None;
	if (button == BTN_LEFT) {
		if (state == 1) {
			type = TB_Mouse1Pressed;
		} else if (state == 0) {
			type = TB_Mouse1Released;
		}
	} else if (button == BTN_RIGHT) {
		if (state == 1) {
			type = TB_Mouse2Pressed;
		} else if (state == 0) {
			type = TB_Mouse2Released;
		}
	}

    struct TaskbarEvent e = {
        .type = type,
        .mouse_x = current_x,
        .mouse_y = current_y,
    };

    taskbar_handle_input_event(taskbar, index, e);
}

static void pointer_axis(void *data, struct wl_pointer *p,
    uint32_t time, uint32_t axis, wl_fixed_t value) {}
static void pointer_frame(void *data, struct wl_pointer *p) {}
static void pointer_axis_source(void *data, struct wl_pointer *p,
    uint32_t source) {}
static void pointer_axis_stop(void *data, struct wl_pointer *p,
    uint32_t time, uint32_t axis) {}
static void pointer_axis_discrete(void *data, struct wl_pointer *p,
    uint32_t axis, int32_t discrete) {}

static const struct wl_pointer_listener pointer_listener = {
    .enter = pointer_enter,
    .leave = pointer_leave,
    .motion = pointer_motion,
    .button = pointer_button,
    .axis = pointer_axis,
    .frame = pointer_frame,
    .axis_source = pointer_axis_source,
    .axis_stop = pointer_axis_stop,
    .axis_discrete = pointer_axis_discrete,
};

/* ================= FRAME ================= */

static void frame_done(void *data, struct wl_callback *cb, uint32_t time) {
    struct BarMonitor *b = data;

    wl_callback_destroy(cb);
    b->frame_cb = NULL;

    if (!b->configured) return;

    struct Buffer *next = &b->buffers[(b->current + 1) % 2];
    if (next->busy) return;

    int index = b - bars;

    int bw = b->width * b->scale;
    int bh = b->height * b->scale;

    taskbar_draw(taskbar, index, next->data, bw, bh, (float)b->scale);

    wl_surface_attach(b->surface, next->wl_buf, 0, 0);
    wl_surface_damage(b->surface, 0, 0, bw, bh);

    b->frame_cb = wl_surface_frame(b->surface);
    static const struct wl_callback_listener listener = { .done = frame_done };
    wl_callback_add_listener(b->frame_cb, &listener, b);

    wl_surface_commit(b->surface);

    next->busy = 1;
    b->current = (b->current + 1) % 2;
}

/* ================= SEAT ================= */

static void seat_capabilities(void *data,
    struct wl_seat *seat,
    uint32_t caps)
{
    if (caps & WL_SEAT_CAPABILITY_POINTER) {
        if (!pointer) {
            pointer = wl_seat_get_pointer(seat);
            wl_pointer_add_listener(pointer, &pointer_listener, NULL);
        }
    } else {
        if (pointer) {
            wl_pointer_destroy(pointer);
            pointer = NULL;
        }
    }
}

static void seat_name(void *data, struct wl_seat *seat, const char *name) {}

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
    .name = seat_name,
};

/* ================= LAYER ================= */

static void layer_configure(void *data,
    struct zwlr_layer_surface_v1 *surface,
    uint32_t serial,
    uint32_t width,
    uint32_t height)
{
    struct BarMonitor *b = data;

    zwlr_layer_surface_v1_ack_configure(surface, serial);

    if (width == 0 || height == 0)
        return;

    b->width = width;
    b->height = height;

    wl_surface_set_buffer_scale(b->surface, b->scale);

    int bw = width * b->scale;
    int bh = height * b->scale;

    struct wl_region *region = wl_compositor_create_region(compositor);
    wl_region_add(region, 0, 0, width, height);
    wl_surface_set_input_region(b->surface, region);
    wl_region_destroy(region);

    create_buffer(&b->buffers[0], bw, bh);
    create_buffer(&b->buffers[1], bw, bh);

    b->current = 0;
    b->configured = 1;

    struct Buffer *buf = &b->buffers[0];
    int index = b - bars;

    taskbar_draw(taskbar, index, buf->data, bw, bh, (float)b->scale);

    wl_surface_attach(b->surface, buf->wl_buf, 0, 0);
    wl_surface_damage(b->surface, 0, 0, bw, bh);

    b->frame_cb = wl_surface_frame(b->surface);
    static const struct wl_callback_listener listener = { .done = frame_done };
    wl_callback_add_listener(b->frame_cb, &listener, b);

    wl_surface_commit(b->surface);

    buf->busy = 1;
}

static void layer_closed(void *data,
    struct zwlr_layer_surface_v1 *surface)
{
    exit(0);
}

static const struct zwlr_layer_surface_v1_listener layer_listener = {
    .configure = layer_configure,
    .closed = layer_closed,
};

/* ================= CREATE BAR ================= */

static void create_bar(struct BarMonitor *b) {
    b->surface = wl_compositor_create_surface(compositor);

    b->layer_surface =
        zwlr_layer_shell_v1_get_layer_surface(
            layer_shell,
            b->surface,
            b->output,
            ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM,
            "taskbar"
        );

    zwlr_layer_surface_v1_set_anchor(
        b->layer_surface,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
    );

    zwlr_layer_surface_v1_set_size(b->layer_surface, 0, BAR_HEIGHT);
    zwlr_layer_surface_v1_set_exclusive_zone(b->layer_surface, BAR_HEIGHT);

    zwlr_layer_surface_v1_set_keyboard_interactivity(
        b->layer_surface,
        ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE
    );

    zwlr_layer_surface_v1_add_listener(b->layer_surface,
        &layer_listener, b);

    wl_surface_commit(b->surface);
}

/* ================= REGISTRY ================= */

static void registry_add(void *data, struct wl_registry *reg,
    uint32_t name, const char *interface, uint32_t version)
{
    if (strcmp(interface, "wl_compositor") == 0)
        compositor = wl_registry_bind(reg, name,
            &wl_compositor_interface, 4);

    else if (strcmp(interface, "wl_shm") == 0)
        shm = wl_registry_bind(reg, name,
            &wl_shm_interface, 1);

    else if (strcmp(interface, "zwlr_layer_shell_v1") == 0)
        layer_shell = wl_registry_bind(reg, name,
            &zwlr_layer_shell_v1_interface, 1);

    else if (strcmp(interface, "wl_seat") == 0) {
        seat = wl_registry_bind(reg, name,
            &wl_seat_interface, 5);
        wl_seat_add_listener(seat, &seat_listener, NULL);
    }

    else if (strcmp(interface, "wl_output") == 0) {
        bars = realloc(bars,
            sizeof(struct BarMonitor) * (n_monitors + 1));

        struct BarMonitor *b = &bars[n_monitors++];
        memset(b, 0, sizeof(*b));

        b->scale = 1;

        b->output = wl_registry_bind(reg, name,
            &wl_output_interface, 2);

        wl_output_add_listener(b->output, &output_listener, b);
    }
}

static void registry_remove(void *data,
    struct wl_registry *registry, uint32_t name) {}

static const struct wl_registry_listener registry_listener = {
    .global = registry_add,
    .global_remove = registry_remove,
};

int main() {
    display = wl_display_connect(NULL);
    if (!display) return 1;

    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry,
        &registry_listener, NULL);

    wl_display_roundtrip(display);

    if (!compositor || !shm || !layer_shell) {
        fprintf(stderr, "Missing Wayland globals\n");
        return 1;
    }

    for (int i = 0; i < n_monitors; i++)
        create_bar(&bars[i]);

    wl_display_roundtrip(display);

    taskbar = taskbar_initialize();

    while (1) {
        wl_display_dispatch(display);
    }

	taskbar_deinitialize(taskbar);

    return 0;
}
