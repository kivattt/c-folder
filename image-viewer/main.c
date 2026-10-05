#define _DEFAULT_SOURCE // What is this?

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

#define IMG_IMPLEMENTATION
#include "img.h"

void usage() {
	printf("Usage: img <image file>\n");
}

struct Renderer {
	unsigned width;
	unsigned height;
	XImage *image;
	XShmSegmentInfo shm;
	uint32_t *pixels;
};

void destroy_renderer(Display *dpy, struct Renderer *r) {
	if (!r->image) {
		return;
	}

	XShmDetach(dpy, &r->shm);
	XDestroyImage(r->image);
	shmdt(r->shm.shmaddr);
	shmctl(r->shm.shmid, IPC_RMID, NULL);
	memset(r, 0, sizeof(*r));
}

// Returns non-zero on error
static int create_renderer(Display *dpy, Visual *visual, unsigned int depth, struct Renderer *r, unsigned int width, unsigned int height) {
	memset(r, 0, sizeof(*r));

	r->width = width;
	r->height = height;
	r->image = XShmCreateImage(dpy, visual, depth, ZPixmap, NULL, &r->shm, width, height);

	if (!r->image) {
		return 0;
	}

	size_t image_size = (size_t)r->image->bytes_per_line * (size_t)r->image->height;

	r->shm.shmid = shmget(IPC_PRIVATE, image_size, IPC_CREAT | 0777);
	if (r->shm.shmid < 0) {
		XDestroyImage(r->image);
		return 0;
	}

	r->shm.shmaddr = shmat(r->shm.shmid, NULL, 0);
	if (r->shm.shmaddr == (char *)-1) {
		shmctl(r->shm.shmid, IPC_RMID, NULL);
		XDestroyImage(r->image);
		return 0;
	}

	r->image->data = r->shm.shmaddr;
	r->shm.readOnly = False;

	if (!XShmAttach(dpy, &r->shm)) {
		shmdt(r->shm.shmaddr);
		shmctl(r->shm.shmid, IPC_RMID, NULL);
		XDestroyImage(r->image);
		return 0;
	}

	XSync(dpy, False);

	/*
		Mark for deletion immediately.
		Segment stays alive until detached.
	*/
	shmctl(r->shm.shmid, IPC_RMID, NULL);
	r->pixels = (uint32_t*)r->image->data;

	return 1;
}

// Returns non-zero on error
int resize_renderer(Display *dpy, Visual *visual, unsigned int depth, struct Renderer *r, unsigned int width, unsigned int height) {
	destroy_renderer(dpy, r);
	return create_renderer(dpy, visual, depth, r, width, height);
}

int main(int argc, char **argv) {
	if (argc < 2) {
		usage();
		return 0;
	}

	char *filename = argv[1];

	struct img_application app;
	int err = img_initialize(&app, filename);
	if (err) {
		return err;
	}

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
	unsigned int width = 1280;
	unsigned int height = 720;

	Window window = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), 100, 100, width, height, 0, 0, 0);
	XStoreName(dpy, window, "img");
	XSelectInput(dpy, window, ExposureMask | StructureNotifyMask | KeyPressMask | PointerMotionMask);
	Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(dpy, window, &wm_delete, 1);

	XMapWindow(dpy, window);
	struct Renderer renderer;

	if (!create_renderer(dpy, visual, (unsigned int)depth, &renderer, width, height)) {
		fprintf(stderr, "Failed to create shared framebuffer\n");
		return 1;
	}

	int running = 1;
	int mouse_x = 0;
	int mouse_y = 0;
	KeySym keysym;

	while (running) {
		while (XPending(dpy)) {
			XEvent e;
			XNextEvent(dpy, &e);
			switch (e.type) {
				case MotionNotify:
					mouse_x = e.xmotion.x;
					mouse_y = e.xmotion.y;
					break;
				case ClientMessage:
					if ((Atom)e.xclient.data.l[0] == wm_delete) {
						running = 0;
					}
					break;

				case ConfigureNotify:
					if (e.xconfigure.width != (int)renderer.width || e.xconfigure.height != (int)renderer.height) {
						resize_renderer(dpy, visual, (unsigned int)depth, &renderer, (unsigned int)e.xconfigure.width, (unsigned int)e.xconfigure.height);
					}
					break;
				case KeyPress:
					keysym = XLookupKeysym(&e.xkey, 0);
					switch (keysym) {
						case XK_Shift_L:
						case XK_Shift_R:
							img_input(&app, IMG_PressShift, mouse_x, mouse_y);
							break;
						case XK_space:
							img_input(&app, IMG_PressSpace, mouse_x, mouse_y);
							break;
						case XK_plus:
							img_input(&app, IMG_PressPlus, mouse_x, mouse_y);
							break;
						case XK_minus:
							img_input(&app, IMG_PressMinus, mouse_x, mouse_y);
							break;
					}

					break;
			}
		}

		img_draw(&app, renderer.pixels, (int)renderer.width, (int)renderer.height);

		XShmPutImage(dpy, window, DefaultGC(dpy, screen), renderer.image, 0, 0, 0, 0, renderer.width, renderer.height, False);
		XSync(dpy, False);
		XFlush(dpy);
	}

	img_deinitialize(&app);
	return 0;
}
