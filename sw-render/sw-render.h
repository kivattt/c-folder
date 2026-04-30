#pragma once

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <immintrin.h>
#include <assert.h>

#include "../fontbmp/fontbmp.h"
#include "font.h"

struct SWRender {
	uint32_t *dest; // Destination image (8-bit ARGB)
	int width;      // Destination image width
	int height;     // Destination image height

	struct FontBMPFont default_font;
	uint32_t last_default_font_size;
};

struct Rect {
	int32_t x, y, w, h;
};

struct FloatRect {
	float x, y, w, h;
};

// Initialization
void swr_initialize(struct SWRender *swr);
void swr_deinitialize(struct SWRender *swr);
void swr_set_output(struct SWRender *swr, uint32_t *buffer, int width, int height);

// Color functions. These return ARGB values
uint32_t swr_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
uint32_t swr_rgb(uint8_t r, uint8_t g, uint8_t b);

// Text measurement function (Doesn't work with swr_draw_text. Only works in conjunction with swr_draw_text_ex right now.)
struct Rect swr_measure_text_ex(struct SWRender *swr, const char *text, struct FontBMPFont *font_bitmaps, uint32_t color, int x, int y); // Get bounding box of text if it were to be drawn.

// Drawing functions
struct Rect swr_draw_text(struct SWRender *swr, const char *text, uint32_t size, uint32_t color, int x, int y); // Draw text using the default font. It regenerates the font bitmaps when size changed since the last call. (Slow!)
struct Rect swr_draw_text_ex(struct SWRender *swr, const char *text, struct FontBMPFont *font_bitmaps, uint32_t color, int x, int y); // Returns bounding box width and height, along with x and y offset relative to the input x y arguments.

void swr_draw_image(struct SWRender *swr, uint32_t *img_argb, int width, int height, int x, int y);
void swr_draw_image_ex(struct SWRender *swr, uint32_t *img_argb, int width, int height, uint32_t color_tint, float scale, int x, int y);

void swr_draw_rectangle(struct SWRender *swr, struct Rect rect, uint32_t color);
void swr_draw_rectangle_rounded(struct SWRender *swr, struct Rect rect, uint32_t color, float radius);
void swr_draw_rectangle_rounded_outline(struct SWRender *swr, struct Rect rect, uint32_t color, float radius, float thickness_inward, float thickness_outward);

// Image channel format conversion
void swr_convert_image_argb_to_abgr(uint32_t *img, int length); // These two are the same.
void swr_convert_image_abgr_to_argb(uint32_t *img, int length); // These two are the same.

// Internal functions
void swr_crash_if_dest_is_null(struct SWRender *swr);
struct Rect swr_draw_text_impl(struct SWRender *swr, const char *text, struct FontBMPFont *font_bitmaps, uint32_t color, int x, int y, int actually_draw);
int swr_draw_glyph(struct SWRender *swr, struct FontBMPGlyphBitmap img, uint32_t color, int img_x, int img_y);
struct Rect swr_rect_intersect(struct Rect a, struct Rect b);
uint32_t swr_abgr_to_argb(uint32_t abgr);
uint32_t swr_alpha_blend(uint32_t dest, uint32_t src);
uint32_t swr_color_tint(uint32_t color, uint32_t tint);
float swr_sdf_rect(float x, float y, struct FloatRect rect, float radius);
float swr_sdf_rect_outline(float x, float y, struct FloatRect rect, float radius, float thickness_inward, float thickness_outward);
float swr_argb_to_float_alpha(uint32_t argb);
uint32_t swr_float_alpha_to_argb(float alpha);
