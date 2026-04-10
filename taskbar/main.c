#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../sw-render/sw-render.h"

#define BAR_HEIGHT 30

typedef struct {
    Window window;
    XImage *fb;
    uint32_t *pixels;
    int x, y, width, height;
} BarMonitor;

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
    BarMonitor *bars = malloc(sizeof(BarMonitor) * res->noutput);

    for (int i = 0; i < res->noutput; i++) {
        XRROutputInfo *output = XRRGetOutputInfo(dpy, res, res->outputs[i]);
        if (output->crtc) {
            XRRCrtcInfo *crtc = XRRGetCrtcInfo(dpy, res, output->crtc);

            int x = crtc->x;
            int y = crtc->y + crtc->height - BAR_HEIGHT;
            int width = crtc->width;
            int height = BAR_HEIGHT;

            uint32_t *pixels = malloc(sizeof(uint32_t) * width * height);
            XImage *fb = XCreateImage(dpy, DefaultVisual(dpy, screen),
                                      DefaultDepth(dpy, screen), ZPixmap, 0,
                                      (char*)pixels, width, height, 32, 0);

            Window win = XCreateSimpleWindow(dpy, root, x, y, width, height, 0,
                                             BlackPixel(dpy, screen),
                                             WhitePixel(dpy, screen));

            XSetWindowAttributes attrs;
            attrs.override_redirect = 1;
            XChangeWindowAttributes(dpy, win, CWOverrideRedirect, &attrs);

            XMapWindow(dpy, win);
            XSelectInput(dpy, win, ButtonPressMask | ExposureMask);

            bars[n_monitors].window = win;
            bars[n_monitors].fb = fb;
            bars[n_monitors].pixels = pixels;
            bars[n_monitors].x = x;
            bars[n_monitors].y = y;
            bars[n_monitors].width = width;
            bars[n_monitors].height = height;
            n_monitors++;

            XRRFreeCrtcInfo(crtc);
        }
        XRRFreeOutputInfo(output);
    }
    XRRFreeScreenResources(res);

	printf("Num monitors: %i\n", n_monitors);

    while (1) {
        usleep(33 * 1000); // 30 fps target

        // --- Draw per monitor ---
        for (int m = 0; m < n_monitors; m++) {
            BarMonitor *b = &bars[m];

			for (int i = 0; i < b->width * b->height; i++) {
				b->pixels[i] = swr_color_to_argb(46, 46, 46);
			}

			// Thin highlight at the top row
			struct Rect rect = (struct Rect){
				.x = 0,
				.y = 0,
				.w = b->width,
				.h = 1,
			};
			swr_draw_rectangle_rounded(b->pixels, b->width, b->height, rect, 0xFF434343, 0.0);

            XPutImage(dpy, b->window, gc, b->fb, 0, 0, 0, 0, b->width, b->height);
        }

        XFlush(dpy);

        // --- Handle events ---
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);

            for (int m = 0; m < n_monitors; m++) {
                if (ev.xany.window == bars[m].window) {
                    switch (ev.type) {
                        case ButtonPress:
                            printf("Monitor %d: Button pressed at (%d,%d)\n",
                                   m, ev.xbutton.x, ev.xbutton.y);
                            break;
                        case Expose:
                            // Redraw immediately if needed
                            XPutImage(dpy, bars[m].window, gc, bars[m].fb,
                                      0, 0, 0, 0, bars[m].width, bars[m].height);
                            break;
                    }
                }
            }
        }
    }

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
