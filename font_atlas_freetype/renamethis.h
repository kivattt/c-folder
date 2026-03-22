#pragma once

#include <ft2build.h>
#include <stdint.h>
#include FT_FREETYPE_H

struct GlyphBitmap {
	int width;
	int height;
	uint8_t *data;
};

//FT_Error generate_font_atlas(const char *font_filename, const int font_height_pixels);
FT_Error generate_font_atlas(struct GlyphBitmap **out_glyph_bitmap_list, uint8_t **out_bitmap_data, const char *font_filename, const int font_height_pixels);
