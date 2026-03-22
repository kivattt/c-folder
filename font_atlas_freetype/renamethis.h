#pragma once

#include <assert.h>

#include <ft2build.h>
#include FT_FREETYPE_H

// Returns non-zero on failure
FT_Error generate_font_atlas(const char *font_filename, const int font_height_pixels) {
	FT_Library library;
	FT_Error error;
	FT_Face face;

	// Initialize freetype2
	error = FT_Init_FreeType(&library);
	if (error) {
		return error;
	}

	error = FT_New_Face(library, font_filename, 0, &face);
	if (error) {
		return error;
	}

	error = FT_Set_Pixel_Sizes(face, 0, font_height_pixels);
	if (error) return error;

	// Generate the font atlas
	for (int character = 0; character < 255; character++) {
		FT_UInt glyph_index = FT_Get_Char_Index(face, character /* FT_ULong, UTF-32 */);
		if (!glyph_index) continue;

		/* https://freetype.org/freetype2/docs/reference/ft2-glyph_retrieval.html#ft_load_xxx */
		error = FT_Load_Glyph(face, glyph_index, 0);
		if (!glyph_index) return 1;

		/* https://freetype.org/freetype2/docs/reference/ft2-glyph_retrieval.html#ft_render_mode */
		error = FT_Render_Glyph(face->glyph, 0);
		if (!glyph_index) return 1;

		int glyph_width = face->glyph->bitmap.width;
		int glyph_height = face->glyph->bitmap.rows;
		if (!face->glyph->bitmap.buffer) {
			continue;
		}

		printf("Character #%i, ptr: %p\n", character, face->glyph->bitmap.buffer);
	}

	return 0;
}
