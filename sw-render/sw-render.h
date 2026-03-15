#pragma once

#include <stdint.h>
#include <immintrin.h>

struct Rect {
	int32_t x, y, w, h;
};

struct Rect rect_intersect(struct Rect a, struct Rect b);

uint32_t fade_opacity_to_black(uint32_t argb);
uint32_t abgr_to_argb(uint32_t abgr);
void convert_image_abgr_to_argb(uint32_t *img, int length);

void draw_image_abgr(uint32_t* buffer, int width, int height, uint32_t* img, int img_width, int img_height, int x, int y);
void draw_image_argb(uint32_t* buffer, int width, int height, uint32_t* img, int img_width, int img_height, int x, int y);
