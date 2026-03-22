#include <assert.h>

#include <ft2build.h>
#include <stdint.h>
#include FT_FREETYPE_H

#include "renamethis.h"

struct FontBitmaps initialize_font_bitmaps() {
	struct FontBitmaps out;
	out.glyph_list = malloc(sizeof(struct GlyphBitmap) * 256);
	out.bitmap_data = NULL;
	return out;
};

void deinitialize_font_bitmaps(struct FontBitmaps font_bitmaps) {
	free(font_bitmaps.glyph_list);
	free(font_bitmaps.bitmap_data);
}

// Frees the font_bitmaps.bitmap_data before re-allocating it.
// Returns non-zero on failure
FT_Error generate_font_atlas(struct FontBitmaps *font_bitmaps, const char *font_filename, const int font_height_pixels) {
	FT_Library library;
	FT_Error error;
	FT_Face face;

	/* Initialize freetype2 */
	error = FT_Init_FreeType(&library);
	if (error) {
		return error;
	}

	error = FT_New_Face(library, font_filename, 0, &face);
	if (error) {
		return error;
	}

	error = FT_Set_Pixel_Sizes(face, 0, font_height_pixels);
	if (error) {
		return error;
	}

	/* Figure out how big the resulting bitmap data will be */
	uint64_t bitmap_size = 0;
	for (int character = 0; character <= 255; character++) {
		FT_UInt glyph_index = FT_Get_Char_Index(face, character /* FT_ULong, UTF-32 */);
		if (!glyph_index) {
			continue;
		}

		error = FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER); // FT_LOAD_RENDER calls FT_Render_Glyph for us.
		if (error || !glyph_index) {
			return 1;
		}

		int glyph_width = face->glyph->bitmap.width;
		int glyph_height = face->glyph->bitmap.rows;
		bitmap_size += glyph_width * glyph_height;
	}

	/* Generate the font atlas */
	free(font_bitmaps->bitmap_data);
	font_bitmaps->bitmap_data = malloc(bitmap_size);
	int index = 0;

	unsigned char *last_bitmap_pointer = NULL;
	for (int character = 0; character <= 255; character++) {
		font_bitmaps->glyph_list[character] = (struct GlyphBitmap){.width = 0, .height = 0, .data = NULL};

		FT_UInt glyph_index = FT_Get_Char_Index(face, character /* FT_ULong, UTF-32 */);
		if (!glyph_index) {
			continue;
		}

		error = FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER); // FT_LOAD_RENDER calls FT_Render_Glyph for us.
		if (error || !glyph_index) {
			return 1;
		}

		int glyph_width = face->glyph->bitmap.width;
		int glyph_height = face->glyph->bitmap.rows;
		if (!face->glyph->bitmap.buffer) {
			continue;
		}

		// If the pointer was the same as the last one, just index into the last one in glyph_bitmap_list
		if (last_bitmap_pointer == face->glyph->bitmap.buffer) {
			font_bitmaps->glyph_list[character] = (struct GlyphBitmap){.width = glyph_width, .height = glyph_height, .data = &font_bitmaps->bitmap_data[index]};
		} else {
			memcpy(&font_bitmaps->bitmap_data[index], face->glyph->bitmap.buffer, glyph_width*glyph_height);
			font_bitmaps->glyph_list[character] = (struct GlyphBitmap){.width = glyph_width, .height = glyph_height, .data = &font_bitmaps->bitmap_data[index]};
			index += glyph_width * glyph_height;
			assert(index <= bitmap_size);
		}

		last_bitmap_pointer = face->glyph->bitmap.buffer;
	}

	FT_Done_Face(face);
	FT_Done_FreeType(library);

	return 0;
}
