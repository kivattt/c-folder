#pragma once

#include <stdint.h>
#include <math.h>
#include <immintrin.h>
#include <assert.h>

#include "../fontbmp/fontbmp.h"

struct Rect {
	int32_t x, y, w, h;
};

struct Rect swr_rect_intersect(struct Rect a, struct Rect b);

uint32_t swr_color_to_argb(uint8_t r, uint8_t g, uint8_t b);
uint32_t swr_abgr_to_argb(uint32_t abgr);
uint32_t swr_alpha_blend(uint32_t dest, uint32_t src);

void swr_convert_image_abgr_to_argb(uint32_t *img, int length);

void swr_draw_image_argb(uint32_t *dest, int dest_width, int dest_height, uint32_t *img, int img_width, int img_height, int img_x, int img_y);

void swr_draw_text(uint32_t *dest, int dest_width, int dest_height, const char *text, struct Font *font_bitmaps, uint32_t text_color, int x, int y);

void swr_draw_rectangle_rounded(uint32_t *dest, int dest_width, int dest_height, struct Rect rect, uint32_t color, float radius);

// Internal functions
int swr_draw_glyph(uint32_t *dest, int dest_width, int dest_height, struct GlyphBitmap img, uint32_t color, int img_x, int img_y);
float swr_sdf_rect(int x, int y, struct Rect rect, float radius);
