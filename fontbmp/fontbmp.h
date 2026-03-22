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

struct FontBitmaps initialize_font_bitmaps();
void deinitialize_font_bitmaps(struct FontBitmaps font_bitmaps);
FT_Error generate_font_bitmaps(struct FontBitmaps *font_bitmaps, const char *font_filename, const int font_height_pixels);
