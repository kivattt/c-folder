#include <X11/X.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#define BAR_HEIGHT 30

Display *dpy;
Window root, bar;
GC gc;
int screen_width;
int screen_height;

int main() {
    dpy = XOpenDisplay(NULL);
    if (!dpy) return 1;

    int screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
    screen_width = DisplayWidth(dpy, screen);
    screen_height = DisplayHeight(dpy, screen);

	screen_width = 2560;
	int x = 0;
	int y = 1440 - BAR_HEIGHT;

    // Create bar
    bar = XCreateSimpleWindow(dpy, root, x, y, screen_width, BAR_HEIGHT, 0,
                              BlackPixel(dpy, screen), WhitePixel(dpy, screen));

	int width = 2560;
	int height = BAR_HEIGHT;
	XImage *fb;
	uint32_t *framebuffer = malloc(4 * width * height);
	fb = XCreateImage(dpy, DefaultVisual(dpy, screen), DefaultDepth(dpy, screen), ZPixmap, 0, (char*)framebuffer, width, height, 32, 0);


    XSetWindowAttributes attrs;
    attrs.override_redirect = 1;
    XChangeWindowAttributes(dpy, bar, CWOverrideRedirect, &attrs);

    XMapWindow(dpy, bar);

    //XSelectInput(dpy, root, SubstructureNotifyMask);
    XSelectInput(dpy, root, SubstructureNotifyMask | ButtonPressMask);

	int frame = 0;
    while (1) {
		++frame;

		//usleep(16 * 1000);
		usleep(8 * 1000);

		// Drawing
		for (int i = 0; i < width*height; i++) {
			framebuffer[i] = 0xFF000000 | (frame & 0xff);
		}
		//XClearWindow(dpy, bar);
		XPutImage(dpy, bar, DefaultGC(dpy, screen), fb, 0, 0, 0, 0, width, height);
		XFlush(dpy);

        // Handle X events (window creation/destruction)
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);

			if (ev.type == ButtonPress) {
				printf("Button press (x: %i, y: %i)!\n", ev.xbutton.x, ev.xbutton.y);
			}
            // Could handle MapNotify/DestroyNotify to redraw immediately
        }
    }

    XCloseDisplay(dpy);
    return 0;
}
