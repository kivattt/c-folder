#include <assert.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdint.h>

#include "fontbmp.h"

struct FontBitmaps fontbmp_initialize() {
	struct FontBitmaps out;
	out.glyph_list = malloc(sizeof(struct GlyphBitmap) * 256);
	out.internal_bitmap_data = NULL;
	return out;
};

void fontbmp_deinitialize(struct FontBitmaps font_bitmaps) {
	free(font_bitmaps.glyph_list);
	free(font_bitmaps.internal_bitmap_data);
}

// Frees the font_bitmaps.bitmap_data before re-allocating it.
// Returns non-zero on failure
FT_Error fontbmp_generate(struct FontBitmaps *font_bitmaps, const char *font_filename, const int font_height_pixels) {
	FT_Library library;
	FT_Error error = 0;
	FT_Face face;

	/* Initialize freetype2 */
	error = FT_Init_FreeType(&library);
	if (error) {
		goto done;
	}

	error = FT_New_Face(library, font_filename, 0, &face);
	if (error) {
		goto done;
	}

	error = FT_Set_Pixel_Sizes(face, 0, font_height_pixels);
	if (error) {
		goto done;
	}

	/* Figure out how big the resulting bitmap data will be */
	int bitmap_size = 0;
	for (int character = 0; character <= 255; character++) {
		error = FT_Load_Char(face, character, FT_LOAD_RENDER); // FT_LOAD_RENDER calls FT_Render_Glyph for us.
		if (error) {
			goto done;
		}

		int width = face->glyph->bitmap.width;
		int pitch = face->glyph->bitmap.pitch;
		int rows = face->glyph->bitmap.rows;
		assert(pitch == width);
		if (!face->glyph->bitmap.buffer) {
			continue;
		}

		bitmap_size += pitch * rows;
	}

	/* Generate the font atlas */
	free(font_bitmaps->internal_bitmap_data);
	font_bitmaps->internal_bitmap_data = malloc(bitmap_size);
	memset(font_bitmaps->internal_bitmap_data, 0, bitmap_size);

	int index = 0;
	for (int character = 0; character <= 255; character++) {
		font_bitmaps->glyph_list[character] = (struct GlyphBitmap){.width = 0, .rows = 0, .bitmap_data = NULL};

		error = FT_Load_Char(face, character, FT_LOAD_RENDER); // FT_LOAD_RENDER calls FT_Render_Glyph for us.
		if (error) {
			goto done;
		}

		if (!face->glyph->bitmap.buffer) {
			continue;
		}

		assert(face->glyph->bitmap.num_grays == 256);
		assert(face->glyph->bitmap.pixel_mode == 2);
		assert(face->glyph->bitmap.palette_mode == 0);
		assert(face->glyph->bitmap.palette == 0);
		assert(face->glyph->bitmap.pitch == face->glyph->bitmap.width);

		int width = face->glyph->bitmap.width;
		int pitch = face->glyph->bitmap.pitch;
		int rows = face->glyph->bitmap.rows;

		assert(pitch >= 0);
		assert(width == pitch);

		// If the pointer was the same as the last one, just index into the last one in glyph_bitmap_list
		memcpy(&font_bitmaps->internal_bitmap_data[index], face->glyph->bitmap.buffer, pitch*rows);
		font_bitmaps->glyph_list[character] = (struct GlyphBitmap){
			.width = width,
			.rows = rows,
			.pitch = pitch,
			.bitmap_data = &(font_bitmaps->internal_bitmap_data[index]),
			.advance_x = face->glyph->advance.x,
			.advance_y = face->glyph->advance.y,
			.bitmap_left = face->glyph->bitmap_left,
			.bitmap_top = face->glyph->bitmap_top,
		};
		index += pitch * rows;
		assert(index <= bitmap_size);
	}

	//printf("actual size: %i, should be: %i\n", index, bitmap_size);
	assert(index == bitmap_size);
done:
	FT_Done_Face(face);
	FT_Done_FreeType(library);

	return error;
}
