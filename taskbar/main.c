#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "taskbar.h"

#define BAR_HEIGHT 30

struct BarMonitor {
    Window window;
    XImage *fb;
    uint32_t *pixels;
    int x, y, width, height;
};

Display *dpy;
Window root;
int screen;
int screen_width, screen_height;
GC gc;

int main() {
    dpy = XOpenDisplay(NULL);
    if (!dpy) return 1;

    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
    screen_width = DisplayWidth(dpy, screen);
    screen_height = DisplayHeight(dpy, screen);

    gc = DefaultGC(dpy, screen);

    // --- Detect monitors with Xrandr ---
    XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);
    int n_monitors = 0;
	struct BarMonitor *bars = malloc(sizeof(struct BarMonitor) * res->noutput);

    for (int i = 0; i < res->noutput; i++) {
        XRROutputInfo *output = XRRGetOutputInfo(dpy, res, res->outputs[i]);
        if (output->crtc) {
            XRRCrtcInfo *crtc = XRRGetCrtcInfo(dpy, res, output->crtc);

            int x = crtc->x;
            int y = crtc->y + crtc->height - BAR_HEIGHT;
            int width = crtc->width;
            int height = BAR_HEIGHT;

            uint32_t *pixels = malloc(sizeof(uint32_t) * width * height);
            XImage *fb = XCreateImage(dpy, DefaultVisual(dpy, screen), DefaultDepth(dpy, screen), ZPixmap, 0, (char*)pixels, width, height, 32, 0);
            Window win = XCreateSimpleWindow(dpy, root, x, y, width, height, 0, BlackPixel(dpy, screen), WhitePixel(dpy, screen));

            XSetWindowAttributes attrs;
            attrs.override_redirect = 1;
            XChangeWindowAttributes(dpy, win, CWOverrideRedirect, &attrs);

            XMapWindow(dpy, win);
            XSelectInput(dpy, win, ExposureMask | ButtonPressMask | PointerMotionMask);

			bars[n_monitors] = (struct BarMonitor){
				.window = win,
				.fb = fb,
				.pixels = pixels,
				.x = x,
				.y = y,
				.width = width,
				.height = height,
			};
            n_monitors++;

            XRRFreeCrtcInfo(crtc);
        }
        XRRFreeOutputInfo(output);
    }
    XRRFreeScreenResources(res);

	/* INITIALIZE TASKBAR */
	printf("Num monitors: %i\n", n_monitors);
	struct Taskbar *taskbar = taskbar_initialize();

    while (1) {
        usleep(33 * 1000); // 30 fps target

        for (int monitor = 0; monitor < n_monitors; monitor++) {
            struct BarMonitor *b = &bars[monitor];
			/* DRAWING */
			taskbar_draw(taskbar, monitor, b->pixels, b->width, b->height);
            XPutImage(dpy, b->window, gc, b->fb, 0, 0, 0, 0, b->width, b->height);
        }

        XFlush(dpy);

        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);

            for (int monitor = 0; monitor < n_monitors; monitor++) {
                if (ev.xany.window == bars[monitor].window) {
					/* INPUT HANDLING */
					struct TaskbarEvent e;
					if (ev.type == ButtonPress) {
						e.type = TB_MouseButton;
					} else if (ev.type == MotionNotify) {
						e.type = TB_MouseMoved;
					}
					e.mouse_x = ev.xbutton.x;
					e.mouse_y = ev.xbutton.y;
					taskbar_handle_input_event(taskbar, monitor, e);

					if (ev.type == Expose) {
						// Redraw immediately if needed
						XPutImage(dpy, bars[monitor].window, gc, bars[monitor].fb, 0, 0, 0, 0, bars[monitor].width, bars[monitor].height);
					}
                }
            }
        }
    }

	taskbar_deinitialize(taskbar);

    // Cleanup (unreachable in this loop, but good practice)
    for (int m = 0; m < n_monitors; m++) {
        XDestroyImage(bars[m].fb);
        free(bars[m].pixels);
        XDestroyWindow(dpy, bars[m].window);
    }
    free(bars);

    XCloseDisplay(dpy);
    return 0;
}
