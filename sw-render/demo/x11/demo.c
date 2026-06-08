#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <sys/ipc.h>
#include <sys/shm.h>

#include <X11/extensions/XShm.h>

#include "../../sw-render.h"

struct Renderer {
    int width;
    int height;
    XImage *image;
    XShmSegmentInfo shm;

    uint32_t *pixels;
};

static void destroy_renderer(Display *dpy, struct Renderer *r) {
    if (!r->image) {
		return;
	}

    XShmDetach(dpy, &r->shm);
    XDestroyImage(r->image);

    shmdt(r->shm.shmaddr);
    shmctl(r->shm.shmid, IPC_RMID, NULL);

    memset(r, 0, sizeof(*r));
}

static bool create_renderer(Display *dpy, Visual *visual, int depth, struct Renderer *r, int width, int height) {
    memset(r, 0, sizeof(*r));

    r->width = width;
    r->height = height;
    r->image = XShmCreateImage(dpy, visual, depth, ZPixmap, NULL, &r->shm, width, height);

    if (!r->image) {
		return false;
	}

    size_t image_size = (size_t)r->image->bytes_per_line * (size_t)r->image->height;

    r->shm.shmid = shmget(IPC_PRIVATE, image_size, IPC_CREAT | 0777);
    if (r->shm.shmid < 0) {
        XDestroyImage(r->image);
        return false;
    }

    r->shm.shmaddr = shmat(r->shm.shmid, NULL, 0);

    if (r->shm.shmaddr == (char *)-1) {
        shmctl(r->shm.shmid, IPC_RMID, NULL);
        XDestroyImage(r->image);
        return false;
    }

    r->image->data = r->shm.shmaddr;
    r->shm.readOnly = False;

    if (!XShmAttach(dpy, &r->shm)) {
        shmdt(r->shm.shmaddr);
        shmctl(r->shm.shmid, IPC_RMID, NULL);
        XDestroyImage(r->image);
        return false;
    }

    XSync(dpy, False);

    /*
        Mark for deletion immediately.

        Segment stays alive until detached.
    */
    shmctl(r->shm.shmid, IPC_RMID, NULL);
    r->pixels = (uint32_t *)r->image->data;

    return true;
}

static bool resize_renderer(Display *dpy, Visual *visual, int depth, struct Renderer *r, int width, int height) {
    destroy_renderer(dpy, r);
    return create_renderer(dpy, visual, depth, r, width, height);
}

int main() {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Failed to open display\n");
        return 1;
    }

    if (!XShmQueryExtension(dpy)) {
        fprintf(stderr, "MIT-SHM not available\n");
        return 1;
    }

    int screen = DefaultScreen(dpy);
    Visual *visual = DefaultVisual(dpy, screen);
    int depth = DefaultDepth(dpy, screen);
    int width = 1280;
    int height = 720;

    Window window = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), 100, 100, width, height, 0, 0, 0);
    XStoreName(dpy, window, "MIT-SHM Software Renderer");
    XSelectInput(dpy, window, ExposureMask | StructureNotifyMask | KeyPressMask);
    Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, window, &wm_delete, 1);

    XMapWindow(dpy, window);
    struct Renderer renderer;

    if (!create_renderer(dpy, visual, depth, &renderer, width, height)) {
        fprintf(stderr, "Failed to create shared framebuffer\n");
        return 1;
    }

	struct SWRender r;
	swr_initialize(&r);

    bool running = true;
	int frame_num = 0;

    while (running) {
        while (XPending(dpy)) {
            XEvent e;
            XNextEvent(dpy, &e);
            switch (e.type) {
                case ClientMessage:
                    if ((Atom)e.xclient.data.l[0] == wm_delete) {
                        running = false;
                    }
                	break;

                case ConfigureNotify:
                    if (e.xconfigure.width != renderer.width || e.xconfigure.height != renderer.height) {
                        resize_renderer(dpy, visual, depth, &renderer, e.xconfigure.width, e.xconfigure.height);
                    }
                	break;
            }
        }

		swr_set_output(&r, renderer.pixels, renderer.width, renderer.height);
		swr_draw_fill_background(&r, swr_rgb(0, 0, 0));
		swr_draw_text(&r, "hello, world!", 22, swr_rgb(255,255,255), 80, 80);
		swr_draw_fps(&r, 22, swr_rgb(0,255,0), 0, 0);
		printf("frame #%i\n", frame_num);
		frame_num += 1;

        XShmPutImage(dpy, window, DefaultGC(dpy, screen), renderer.image, 0, 0, 0, 0, renderer.width, renderer.height, False);
		XSync(dpy, False);
        XFlush(dpy);
    }

    destroy_renderer(dpy, &renderer);

    XDestroyWindow(dpy, window);
    XCloseDisplay(dpy);

	swr_deinitialize(&r);

    return 0;
}
