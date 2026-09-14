#ifndef SWR_H
#define SWR_H

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ft2build.h>
#include FT_FREETYPE_H

struct swr_glyph_bitmap {
	unsigned int width;
	unsigned int rows; // height
	unsigned int pitch; // byte offset to get the next row
	uint8_t *bitmap_data;

	// Advance x and y from Freetype2, (shift right by 6 to get pixel amount)
	signed long advance_x;
	signed long advance_y;

	signed int bitmap_left;
	signed int bitmap_top;
};

struct swr_font {
	struct swr_glyph_bitmap *glyph_list;
	uint8_t *internal_bitmap_data;

	// Ascender from Freetype2 (shift right by 6 to get pixel amount)
	signed long ascender;
};

struct swr_output {
	uint32_t *dest; // Destination image (8-bit ARGB)
	int width;
	int height;

	struct swr_font default_font;
	int32_t last_default_font_size;
};

/* PUBLIC FUNCTIONS */

void swr_initialize(struct swr_output *swr);
void swr_deinitialize(struct swr_output *swr);
void swr_set_output(struct swr_output *swr, uint32_t *dest, int width, int height);

/* FONT BITMAP GENERATION FUNCTIONS */
struct swr_font swr_fontbmp_initialize(); // Allocates enough for the glyph_list (256 elements)
void swr_fontbmp_deinitialize(struct swr_font font);
// These two functions free the font.bitmap_data before re-allocating it.
// font_height_pixels sets the height of the EM square in pixels. Characters will usually appear smaller than specified, but could even be larger!
FT_Error swr_fontbmp_generate(struct swr_font *font, const char *font_filename, const unsigned int font_height_pixels);
FT_Error swr_fontbmp_generate_from_memory(struct swr_font *font, const unsigned char *font_data, size_t font_data_size, const unsigned int font_height_pixels);

/* END OF PUBLIC FUNCTIONS */

/* IMPLEMENTATION */
#define USE_DURING_DEVELOPMENT // This is just here to prevent my vim syntax highlighting from greying out the implementation.
#if defined(SWR_IMPLEMENTATION) || defined(USE_DURING_DEVELOPMENT)

void swr_initialize(struct swr_output *swr) {
#ifdef USE_DURING_DEVELOPMENT
	printf("\x1b[31mRemember to remove USE_DURING_DEVELOPMENT !\x1b[0m\n");
#endif
	if (swr == NULL) {
		printf("swr: swr_initialize called with a NULL pointer. Remember: your swr_output struct should be on the stack!\n");
		assert(0);
	}
	memset(swr, 0, sizeof(struct swr_output));
	swr->last_default_font_size = -1;
	swr->default_font = swr_fontbmp_initialize();
}

void swr_deinitialize(struct swr_output *swr) {
	assert(swr != NULL);
	swr_fontbmp_deinitialize(swr->default_font);
}

void swr_set_output(struct swr_output *swr, uint32_t *dest, int width, int height) {
	// We allow swr to be NULL.
	// Other functions check for NULL and return early instead of here.
	assert(width >= 0);
	assert(height >= 0);
	swr->dest = dest;
	swr->width = width;
	swr->height = height;
}

/* FONT BITMAP FUNCTIONS */
struct swr_font swr_fontbmp_initialize() {
#ifdef USE_DURING_DEVELOPMENT
	printf("\x1b[31mRemember to remove USE_DURING_DEVELOPMENT !\x1b[0m\n");
#endif
	struct swr_font out;
	out.glyph_list = malloc(sizeof(struct swr_glyph_bitmap) * 256);
	out.internal_bitmap_data = NULL;
	return out;
}

void swr_fontbmp_deinitialize(struct swr_font font) {
	free(font.glyph_list);
	free(font.internal_bitmap_data);
}

// Frees the font.bitmap_data before re-allocating it.
// Returns non-zero on failure
FT_Error swr_fontbmp_generate(struct swr_font *font, const char *font_filename, const unsigned int font_height_pixels) {
	assert(font != NULL);

	int fd = open(font_filename, O_RDONLY);
	if (fd == -1) {
		printf("swr_fontbmp_generate: Failed to open font file: %s\n", font_filename);
		return 1;
	}

	struct stat st;
	if (fstat(fd, &st) == -1) {
		printf("swr_fontbmp_generate: Failed to stat font file: %s\n", font_filename);
		close(fd);
		return 1;
	}

	size_t font_data_size = (size_t)st.st_size;
	unsigned char *font_data = mmap(NULL, font_data_size, PROT_READ, MAP_SHARED, fd, 0);
	FT_Error err = swr_fontbmp_generate_from_memory(font, font_data, font_data_size, font_height_pixels);

	close(fd);
	munmap(font_data, font_data_size);
	return err;
}

// Frees the font.bitmap_data before re-allocating it.
// Returns non-zero on failure
FT_Error swr_fontbmp_generate_from_memory(struct swr_font *font, const unsigned char *font_data, size_t font_data_size, const unsigned int font_height_pixels) {
	assert(font != NULL);

	FT_Library library;
	FT_Error error = 0;
	FT_Face face;

	/* Initialize freetype2 */
	error = FT_Init_FreeType(&library);
	if (error) {
		goto done;
	}

	error = FT_New_Memory_Face(library, font_data, (FT_Long)font_data_size, 0, &face);
	if (error) {
		goto done;
	}

	error = FT_Set_Pixel_Sizes(face, 0, font_height_pixels);
	if (error) {
		goto done;
	}

	/* Figure out how big the resulting bitmap data will be */
	unsigned int bitmap_size = 0;
	for (int character = 0; character <= 255; character++) {
		if (character == ' ' || character == '\t' || character == '\n') {
			continue;
		}

		error = FT_Load_Char(face, (FT_ULong)character, FT_LOAD_RENDER); // FT_LOAD_RENDER calls FT_Render_Glyph for us.
		if (error) {
			goto done;
		}

		unsigned int width = face->glyph->bitmap.width;
		unsigned int pitch = (unsigned int)face->glyph->bitmap.pitch;
		unsigned int rows = face->glyph->bitmap.rows;
		assert(pitch == width);
		if (!face->glyph->bitmap.buffer) {
			continue;
		}

		bitmap_size += pitch * rows;
	}

	/* Generate the font atlas */
	free(font->internal_bitmap_data);
	font->internal_bitmap_data = malloc(bitmap_size);
	memset(font->internal_bitmap_data, 0, bitmap_size);

	unsigned int index = 0;
	for (int character = 0; character <= 255; character++) {
		font->glyph_list[character] = (struct swr_glyph_bitmap){.width = 0, .rows = 0, .bitmap_data = NULL};

		if (character == ' ') {
			error = FT_Load_Char(face, ' ', FT_LOAD_DEFAULT);
			font->glyph_list[character] = (struct swr_glyph_bitmap){
				.advance_x = face->glyph->advance.x,
				.advance_y = face->glyph->advance.y,
				.bitmap_data = NULL,
			};
			continue;
		} else if (character == '\t') {
			error = FT_Load_Char(face, ' ', FT_LOAD_DEFAULT);
			font->glyph_list[character] = (struct swr_glyph_bitmap){
				.advance_x = face->glyph->advance.x * 4, // 4 spaces
				.advance_y = 0,
				.bitmap_data = NULL,
			};
			continue;
		} else if (character == '\n') {
			error = FT_Load_Char(face, ' ', FT_LOAD_DEFAULT);
			font->glyph_list[character] = (struct swr_glyph_bitmap){
				.advance_x = 0, // It's up to the renderer to reset x position.
				.advance_y = face->size->metrics.height,
				.bitmap_data = NULL,
			};
			continue;
		}

		error = FT_Load_Char(face, (FT_ULong)character, FT_LOAD_RENDER); // FT_LOAD_RENDER calls FT_Render_Glyph for us.
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
		assert((unsigned int)face->glyph->bitmap.pitch == face->glyph->bitmap.width);

		unsigned int width = face->glyph->bitmap.width;
		unsigned int pitch = (unsigned int)face->glyph->bitmap.pitch;
		unsigned int rows = face->glyph->bitmap.rows;

		assert(width == pitch);

		memcpy(&font->internal_bitmap_data[index], face->glyph->bitmap.buffer, pitch*rows);
		font->glyph_list[character] = (struct swr_glyph_bitmap){
			.width = width,
			.rows = rows,
			.pitch = pitch,
			.bitmap_data = &(font->internal_bitmap_data[index]),
			.advance_x = face->glyph->advance.x,
			.advance_y = face->glyph->advance.y,
			.bitmap_left = face->glyph->bitmap_left,
			.bitmap_top = face->glyph->bitmap_top,
		};
		index += pitch * rows;
		assert(index <= bitmap_size);
	}

	assert(error == 0);
	font->ascender = face->size->metrics.ascender;

	//printf("actual size: %i, should be: %i\n", index, bitmap_size);
	assert(index == bitmap_size);
done:
	FT_Done_Face(face);
	FT_Done_FreeType(library);


	return error;
}

/* END OF FONT BITMAP FUNCTIONS */

#endif // SWR_IMPLEMENTATION
/* END OF IMPLEMENTATION */

#endif // SWR_H
