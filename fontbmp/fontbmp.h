#pragma once

#include <ft2build.h>
#include <stdint.h>
#include FT_FREETYPE_H

struct GlyphBitmap {
	int width;
	int height;
	uint8_t *data;
};

struct FontBitmaps {
	struct GlyphBitmap *glyph_list;
	uint8_t *bitmap_data;
};

// Initialize the library
// Allocates enough for the glyph_list (256 elements)
struct FontBitmaps initialize_font_bitmaps();

// Deinitialize the library
void deinitialize_font_bitmaps(struct FontBitmaps font_bitmaps);

// font_height_pixels is not actually a measure in pixels, its some Freetype2 bs. A bitmap height can go beyond what you specify.
FT_Error generate_font_bitmaps(struct FontBitmaps *font_bitmaps, const char *font_filename, const int font_height_pixels);
