#pragma once

#include <ft2build.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include FT_FREETYPE_H

struct FontBMPGlyphBitmap {
	int width;
	int rows; // height
	int pitch; // byte offset to get the next row
	uint8_t *bitmap_data;

	// Advance x and y from Freetype2, (shift right by 6 to get pixel amount)
	signed long advance_x;
	signed long advance_y;

	signed int bitmap_left;
	signed int bitmap_top;
};

struct FontBMPFont {
	struct FontBMPGlyphBitmap *glyph_list;
	uint8_t *internal_bitmap_data;
};

// Initialize the library
// Allocates enough for the glyph_list (256 elements)
struct FontBMPFont fontbmp_initialize();

// Deinitialize the library
void fontbmp_deinitialize(struct FontBMPFont font_bitmaps);

// font_height_pixels is not actually a measure in pixels, its some Freetype2 bs. A bitmap height can go beyond what you specify.
FT_Error fontbmp_generate(struct FontBMPFont *font_bitmaps, const char *font_filename, const int font_height_pixels);

// font_height_pixels is not actually a measure in pixels, its some Freetype2 bs. A bitmap height can go beyond what you specify.
FT_Error fontbmp_generate_from_memory(struct FontBMPFont *font_bitmaps, const unsigned char *font_data, signed long font_data_size, const int font_height_pixels);
