//#undef NDEBUG

#include "sw-render.h"

#ifdef __clang__
	#define ROTR(a, b) __builtin_rotateright32((a), (b))
#else
	#define ROTR(a, b) _rotr((a), (b))
#endif

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// Returns the intersection of two rectangles.
// If there is no intersection, it returns an empty rectangle with x=0, y=0, w=0, h=0
struct Rect swr_rect_intersect(struct Rect a, struct Rect b) {
	int32_t x1 = MAX(a.x, b.x);
	int32_t x2 = MIN(a.x + a.w, b.x + b.w);
	int32_t y1 = MAX(a.y, b.y);
	int32_t y2 = MIN(a.y + a.h, b.y + b.h);

	struct Rect out;
	if (x2 >= x1 && y2 >= y1) {
		out = (struct Rect){.x = x1, .y = y1, .w = x2-x1, .h = y2-y1};
	} else {
		out = (struct Rect){.x = 0, .y = 0, .w = 0, .h = 0};
	}

	return out;
}

uint32_t swr_abgr_to_argb(uint32_t abgr) {
	return ROTR(__builtin_bswap32(abgr), 8);
}

uint32_t swr_fade_opacity_to_black(uint32_t argb) {
	uint8_t alpha = argb >> 24; // 0 to 255
	uint16_t r = (argb >> 16) & 0xff;
	uint16_t g = (argb >> 8) & 0xff;
	uint16_t b = (argb >> 0) & 0xff;

	r *= alpha;
	g *= alpha;
	b *= alpha;

	r /= 255;
	g /= 255;
	b /= 255;

	assert(r < 256);
	assert(g < 256);
	assert(b < 256);

	return 0xFF000000 | r << 16 | g << 8 | b;
}

// Draws an ABGR img to the buffer at position img_x, img_y
// Outputs in ARGB format.
void swr_draw_image_abgr(uint32_t *dest, int dest_width, int dest_height, uint32_t *img, int img_width, int img_height, int img_x, int img_y) {
	struct Rect buffer_rect = {.x = 0,     .y = 0,     .w = dest_width, .h = dest_height};
	struct Rect img_rect =    {.x = img_x, .y = img_y, .w = img_width,    .h = img_height};

	int x_offset = 0, y_offset = 0;
	if (img_x < 0) x_offset = -img_x;
	if (img_y < 0) y_offset = -img_y;

	struct Rect visible = swr_rect_intersect(buffer_rect, img_rect);
	assert(visible.x >= 0);
	assert(visible.y >= 0);
	assert(visible.w >= 0);
	assert(visible.h >= 0);

	for (int y = 0; y < visible.h; y++) {
		for (int x = 0; x < visible.w; x++) {
			// Fetch the pixel from img
			int img_sample_x = x + x_offset;
			int img_sample_y = y + y_offset;
			int img_index = img_sample_y * img_width + img_sample_x;
			uint32_t img_color = swr_fade_opacity_to_black(swr_abgr_to_argb(img[img_index])); // Slow memory fetch bottleneck

			// Set the pixel
			int buffer_index = (visible.y+y) * dest_width + (visible.x+x);
			dest[buffer_index] = img_color;
		}
	}
}

// Converts an image from ABGR to ARGB in-place
// Fades opacity to black
void swr_convert_image_abgr_to_argb(uint32_t *img, int length) {
	for (int i = 0; i < length; i++) {
		img[i] = swr_fade_opacity_to_black(swr_abgr_to_argb(img[i]));
	}
}

// Converts a single-channel 8-bit alpha image to ARGB
// This function allocates and returns a new image. Remember to free it!
uint32_t *swr_convert_image_a_to_argb(uint8_t* img, int length) {
	uint32_t *output = (uint32_t*)malloc(4 * length);

	for (int i = 0; i < length; i++) {
		output[i] = swr_fade_opacity_to_black(img[i] << 24 | 0x00FFFFFF);
	}

	return output;
}

void swr_draw_image_argb(uint32_t *dest, int dest_width, int dest_height, uint32_t *img, int img_width, int img_height, int img_x, int img_y) {
	struct Rect buffer_rect = {.x = 0,     .y = 0,     .w = dest_width, .h = dest_height};
	struct Rect img_rect =    {.x = img_x, .y = img_y, .w = img_width,    .h = img_height};

	int x_offset = 0, y_offset = 0;
	if (img_x < 0) x_offset = -img_x;
	if (img_y < 0) y_offset = -img_y;

	struct Rect visible = swr_rect_intersect(buffer_rect, img_rect);
	assert(visible.x >= 0);
	assert(visible.y >= 0);
	assert(visible.w >= 0);
	assert(visible.h >= 0);

	for (int y = 0; y < visible.h; y++) {
		int img_sample_x = x_offset;
		int img_sample_y = y + y_offset;
		int img_index = img_sample_y * img_width + img_sample_x;

		int dest_index = (visible.y+y) * dest_width + (visible.x);
		memcpy(dest+dest_index, img+img_index, visible.w * 4);
	}
}

// Draws a single-channel 8-bit alpha image to dest, outputs in ARGB
// Returns non-zero if it would draw outside of the screen (nothing to draw)
int swr_draw_glyph(uint32_t *dest, int dest_width, int dest_height, struct GlyphBitmap img, uint32_t color, int img_x, int img_y) {
	struct Rect buffer_rect = {.x = 0,     .y = 0,     .w = dest_width,  .h = dest_height};
	struct Rect img_rect =    {.x = img_x, .y = img_y, .w = img.width, .h = img.rows};

	int x_offset = 0, y_offset = 0;
	if (img_x < 0) x_offset = -img_x;
	if (img_y < 0) y_offset = -img_y;

	struct Rect visible = swr_rect_intersect(buffer_rect, img_rect);
	assert(visible.x >= 0);
	assert(visible.y >= 0);
	assert(visible.w >= 0);
	assert(visible.h >= 0);

	// Nothing to draw
	if (visible.w == 0 && visible.h == 0) {
		return 1;
	}

	for (int y = 0; y < visible.h; y++) {
		for (int x = 0; x < visible.w; x++) {
			// Fetch the pixel from img
			int img_sample_x = x + x_offset;
			int img_sample_y = y + y_offset;
			assert(img.pitch == img.width);
			int img_index = img_sample_y * img.pitch + img_sample_x;

			// TODO: Alpha blending
			uint32_t img_color = swr_fade_opacity_to_black(img.bitmap_data[img_index] << 24 | color); // Slow memory fetch bottleneck

			// Set the pixel
			int buffer_index = (visible.y+y) * dest_width + (visible.x+x);
			dest[buffer_index] = img_color;
		}
	}

	return 0;
}

void swr_draw_text(uint32_t *dest, int dest_width, int dest_height, const char *text, struct Font *font_bitmaps, int x, int y) {
	int pen_x = 0;
	int pen_y = 0;

	for (int i = 0; i < strlen(text); i++) {
		unsigned char c = *(unsigned char*)&text[i]; // Need to reinterpret signed char as unsigned.

		struct GlyphBitmap glyph = font_bitmaps->glyph_list[c];
		if (glyph.bitmap_data == NULL) {
			pen_x += glyph.advance_x >> 6;
			pen_y += glyph.advance_y >> 6;

			if (c == '\n') {
				pen_x = 0;
			}
			continue;
		}

		int x_pos = x + pen_x + glyph.bitmap_left;
		int y_pos = y + pen_y - glyph.bitmap_top;

		if (swr_draw_glyph(dest, dest_width, dest_height, glyph, 0x00FFFFFF, x_pos, y_pos)) {
			// Nothing to draw beyond this line.
			// FIXME: Skip to the next line. Also return if we drew the last visible line.
			continue;
		}

		pen_x += glyph.advance_x >> 6;
		pen_y += glyph.advance_y >> 6;
	}
}
