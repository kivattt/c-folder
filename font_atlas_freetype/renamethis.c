#include <assert.h>

#include <ft2build.h>
#include <stdint.h>
#include FT_FREETYPE_H

#include "renamethis.h"

#define ENOUGH_FOR_INTER_REGULAR_TTF 82564

// Hardcoded to only work with Inter-Regular.ttf
// Remember to free(out_glyph_bitmap_list); free(out_bitmap_data);
// Returns non-zero on failure
FT_Error generate_font_atlas(struct GlyphBitmap **out_glyph_bitmap_list, uint8_t **out_bitmap_data, const char *font_filename, const int font_height_pixels) {
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

	/* Generate the font atlas */
	struct GlyphBitmap *glyph_bitmap_list = malloc(sizeof(struct GlyphBitmap) * 256);
	*out_glyph_bitmap_list = glyph_bitmap_list;

	uint8_t *bitmap_data = malloc(ENOUGH_FOR_INTER_REGULAR_TTF);
	*out_bitmap_data = bitmap_data;
	int index = 0;

	unsigned char *last_bitmap_pointer = NULL;
	for (int character = 0; character <= 255; character++) {
		glyph_bitmap_list[character] = (struct GlyphBitmap){.width = 0, .height = 0, .data = NULL};

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

		// If the pointer was the same as the last one, just index into the last one in glyph_bitmap_list
		if (last_bitmap_pointer == face->glyph->bitmap.buffer) {
			glyph_bitmap_list[character] = (struct GlyphBitmap){.width = glyph_width, .height = glyph_height, .data = &bitmap_data[index]};
		} else {
			memcpy(&bitmap_data[index], face->glyph->bitmap.buffer, glyph_width*glyph_height);
			glyph_bitmap_list[character] = (struct GlyphBitmap){.width = glyph_width, .height = glyph_height, .data = &bitmap_data[index]};
			index += glyph_width * glyph_height;
			assert(index <= ENOUGH_FOR_INTER_REGULAR_TTF);
		}

		//printf("Character #%i, ptr: %p\n", character, face->glyph->bitmap.buffer);

		last_bitmap_pointer = face->glyph->bitmap.buffer;
	}

	FT_Done_Face(face);
	FT_Done_FreeType(library);

	//printf("sum bytes: %i\n", index);

	return 0;
}

#undef ENOUGH_FOR_INTER_REGULAR_TTF
