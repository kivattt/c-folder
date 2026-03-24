#pragma once

#include <stdint.h>
#include <immintrin.h>
#include <assert.h>

#include "../fontbmp/fontbmp.h"

struct Rect {
	int32_t x, y, w, h;
};

struct Rect rect_intersect(struct Rect a, struct Rect b);

uint32_t fade_opacity_to_black(uint32_t argb);
uint32_t abgr_to_argb(uint32_t abgr);

void convert_image_abgr_to_argb(uint32_t *img, int length);
uint32_t *convert_image_a_to_argb(uint8_t* img, int length);

void draw_image_abgr(uint32_t* buffer, int width, int height, uint32_t* img, int img_width, int img_height, int x, int y);
void draw_image_argb(uint32_t* buffer, int width, int height, uint32_t* img, int img_width, int img_height, int x, int y);
void draw_image_a(uint32_t *dest, int dest_width, int dest_height, uint8_t *img, int img_width, int img_height, uint32_t color, int img_x, int img_y);

void draw_text(uint32_t *dest, int dest_width, int dest_height, const char *text, struct FontBitmaps *font_bitmaps, int x, int y);
