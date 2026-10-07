#ifndef IMG_H
#define IMG_H

#include <assert.h>

#if defined(__TINYC__)
#define STBI_NO_SIMD
#endif
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define SWR_DEBUG_INFO
#define SWR_IMPLEMENTATION
#include "../swr/swr.h"

struct img_application {
	struct swr_output r;
	int show_fps;
	float scale;

	uint32_t *image;
	int image_width;
	int image_height;
	int image_channel_count;
};

enum img_input_event {
	IMG_PressShift,
	IMG_PressSpace,
	IMG_PressPlus,
	IMG_PressMinus,
};

/* PUBLIC FUNCTIONS */
int img_initialize(struct img_application *app, char *image_filename);
void img_deinitialize(struct img_application *app);
void img_input(struct img_application *app, enum img_input_event event, int mouse_x, int mouse_y);
void img_draw(struct img_application *app, uint32_t *framebuf, int width, int height);
/* END OF PUBLIC FUNCTIONS */

/* IMPLEMENTATION */
#define IMG_USE_DURING_DEVELOPMENT
#if defined(IMG_IMPLEMENTATION) || defined(IMG_USE_DURING_DEVELOPMENT)

// Returns non-zero on error
int img_initialize(struct img_application *app, char *image_filename) {
#ifdef IMG_USE_DURING_DEVELOPMENT
	printf("\x1b[31mRemember to remove IMG_USE_DURING_DEVELOPMENT !\x1b[0m\n");
#endif
	memset(app, 0, sizeof(struct img_application));

	swr_initialize(&app->r);
	app->scale = 1.0F;
	app->image = (uint32_t*)stbi_load(image_filename, &app->image_width, &app->image_height, &app->image_channel_count, 4);
	if (app->image == NULL) {
		printf("Failed to load %s\n", image_filename);
		return 1;
	}
	swr_convert_image_abgr_to_argb(app->image, app->image_width, app->image_height);
	double start = swr__time_ms();
	//swr_blur_image(app->image, app->image_width, app->image_height);
	//for (int i = 0; i < 10; i++)
	swr_blur_image2(app->image, app->image_width, app->image_height);
	double duration = swr__time_ms() - start;
	printf("took: %fms\n", duration);
	//exit(0);
	return 0;
}

void img_deinitialize(struct img_application *app) {
	stbi_image_free(app->image);
	swr_deinitialize(&app->r);
}

void img_input(struct img_application *app, enum img_input_event event, int mouse_x, int mouse_y) {
	switch (event) {
		case IMG_PressShift:
			app->show_fps = !app->show_fps;
			break;
		case IMG_PressSpace:
			app->scale = 1.0F;
			break;
		case IMG_PressPlus:
			app->scale += 0.1F;
			break;
		case IMG_PressMinus:
			app->scale -= 0.1F;
			break;
	}
}

void img_draw(struct img_application *app, uint32_t *framebuf, int width, int height) {
	swr_set_output(&app->r, framebuf, width, height);
	swr_draw_fill(&app->r, swr_rgb(50,50,50));

	swr_draw_image_ex(&app->r, app->image, app->image_width, app->image_height, swr_rgb(255,255,255), app->scale, 0, 0);

	swr_draw_fps(&app->r, 22, swr_rgb(100,255,100), 0, 0, app->show_fps);
}

#endif
/* END OF IMPLEMENTATION */

#endif // IMG_H
