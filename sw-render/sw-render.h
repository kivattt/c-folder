#pragma once

#include <stdint.h>
#include <immintrin.h>
#include <assert.h>

#include "../fontbmp/fontbmp.h"

struct Rect {
	int32_t x, y, w, h;
};

struct Rect swr_rect_intersect(struct Rect a, struct Rect b);

uint32_t swr_fade_opacity_to_black(uint32_t argb);
uint32_t swr_abgr_to_argb(uint32_t abgr);

void swr_convert_image_abgr_to_argb(uint32_t *img, int length);
uint32_t *swr_convert_image_a_to_argb(uint8_t* img, int length);

void swr_draw_image_abgr(uint32_t *dest, int dest_width, int dest_height, uint32_t *img, int img_width, int img_height, int img_x, int img_y);
void swr_draw_image_argb(uint32_t *dest, int dest_width, int dest_height, uint32_t *img, int img_width, int img_height, int img_x, int img_y);
//void swr_draw_image_a   (uint32_t *dest, int dest_width, int dest_height, uint8_t  *img, int img_width, int img_height, uint32_t img_color, int img_x, int img_y);

void swr_draw_text(uint32_t *dest, int dest_width, int dest_height, const char *text, struct FontBitmaps *font_bitmaps, int x, int y);

// Internal functions
int swr_draw_glyph(uint32_t *dest, int dest_width, int dest_height, struct GlyphBitmap img, uint32_t color, int img_x, int img_y);
