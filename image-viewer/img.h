#ifndef IMG_H
#define IMG_H

#include <assert.h>

struct img_application {
	
};

/* PUBLIC FUNCTIONS */

void img_initialize();
void img_deinitialize();
void img_input();
void img_draw();

/* END OF PUBLIC FUNCTIONS */

/* IMPLEMENTATION */
#define IMG_USE_DURING_DEVELOPMENT
#if defined(IMG_IMPLEMENTATION) || defined(IMG_USE_DURING_DEVELOPMENT)

void img_initialize() {
}

void img_deinitialize() {
}

void img_input() {
}

void img_draw() {
	swr_set_output(&r, renderer.pixels, (int)renderer.width, (int)renderer.height);
	swr_draw_fill(&r, swr_rgb(50,50,50));
	//swr_draw_image(&r, (uint32_t*)image, img_width, img_height, mouse_x, mouse_y);
	swr_draw_image_ex(&r, (uint32_t*)image, img_width, img_height, swr_rgb(255,255,255), scale, mouse_x, mouse_y);

	swr_draw_fps(&r, 22, swr_rgb(255,255,255), 0, 0, show_fps);
}

#endif
/* END OF IMPLEMENTATION */

#endif // IMG_H
