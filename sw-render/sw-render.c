//#undef NDEBUG

#include "sw-render.h"

#ifdef __clang__
	#define ROTR(a, b) __builtin_rotateright32((a), (b))
#else
	#define ROTR(a, b) _rotr((a), (b))
#endif

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

void swr_initialize(struct SWRender *swr) {
	memset(swr, 0, sizeof(struct SWRender));
}

void swr_set_output(struct SWRender *swr, uint32_t *dest, int width, int height) {
	swr->dest = dest;
	swr->width = width;
	swr->height = height;
}

// FIXME: Output to stderr
void swr_crash_if_dest_is_null(struct SWRender *swr) {
	if (swr->dest == NULL) {
		printf("sw-render: dest was NULL, did you forget to call swr_set_output() ?\n");
		assert(0);
	}
}

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

uint32_t swr_color_to_argb(uint8_t r, uint8_t g, uint8_t b) {
	return 0xFF000000 | r << 16 | g << 8 | b;
}

uint32_t swr_abgr_to_argb(uint32_t abgr) {
	return ROTR(__builtin_bswap32(abgr), 8);
}

float swr_argb_to_float_alpha(uint32_t argb) {
	return (float)(argb >> 24) / 255.0;
}

uint32_t swr_float_alpha_to_argb(float alpha) {
	return (uint8_t)(alpha * 255.0) << 24;
}

// 8-bit ARGB colors
uint32_t swr_alpha_blend(uint32_t dest, uint32_t src) {
	dest |= 0xFF000000;

	uint8_t a = src >> 24;
	uint8_t r = (((src >> 16) & 0xFF) * a) / 255 + (((dest >> 16) & 0xFF) * (255 - a)) / 255;
	uint8_t g = (((src >>  8) & 0xFF) * a) / 255 + (((dest >>  8) & 0xFF) * (255 - a)) / 255;
	uint8_t b = (((src >>  0) & 0xFF) * a) / 255 + (((dest >>  0) & 0xFF) * (255 - a)) / 255;

	return 0xFF000000 | r << 16 | g << 8 | b;
}

// Converts an image from ABGR to ARGB in-place
void swr_convert_image_abgr_to_argb(uint32_t *img, int length) {
	for (int i = 0; i < length; i++) {
		img[i] = swr_abgr_to_argb(img[i]);
	}
}

void swr_draw_image_argb(struct SWRender *swr, uint32_t *img, int img_width, int img_height, int img_x, int img_y) {
	swr_crash_if_dest_is_null(swr);

	struct Rect buffer_rect = {.x = 0,     .y = 0,     .w = swr->width, .h = swr->height};
	struct Rect img_rect =    {.x = img_x, .y = img_y, .w = img_width,  .h = img_height};

	struct Rect visible = swr_rect_intersect(buffer_rect, img_rect);
	int x_offset = -MIN(0, img_x);
	int y_offset = -MIN(0, img_y);

	for (int y = 0; y < visible.h; y++) {
		int img_sample_x = x_offset;
		int img_sample_y = y + y_offset;
		int img_index = img_sample_y * img_width + img_sample_x;

		int dest_index = (visible.y+y) * swr->width + (visible.x);
		//memcpy(dest+dest_index, img+img_index, visible.w * 4);

		for (int x = 0; x < visible.w; x++) {
			swr->dest[dest_index + x] = swr_alpha_blend(swr->dest[dest_index + x], img[img_index + x]);
		}
	}
}

// Draws a single-channel 8-bit alpha image to dest, outputs in ARGB
// Returns non-zero if it would draw outside of the screen (nothing to draw)
int swr_draw_glyph(struct SWRender *swr, struct GlyphBitmap img, uint32_t color, int img_x, int img_y) {
	struct Rect buffer_rect = {.x = 0,     .y = 0,     .w = swr->width, .h = swr->height};
	struct Rect img_rect =    {.x = img_x, .y = img_y, .w = img.width,  .h = img.rows};

	struct Rect visible = swr_rect_intersect(buffer_rect, img_rect);
	int x_offset = -MIN(0, img_x);
	int y_offset = -MIN(0, img_y);

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

			int buffer_index = (visible.y+y) * swr->width + (visible.x+x);
			uint8_t alpha = (float)(img.bitmap_data[img_index] / 255.0) * (float)(color >> 24);
			uint32_t img_color = swr_alpha_blend(swr->dest[buffer_index], alpha << 24 | (color & 0x00FFFFFF));

			// Set the pixel
			swr->dest[buffer_index] = img_color;
		}
	}

	return 0;
}

// text_color is an 8-bit ARGB value.
void swr_draw_text(struct SWRender *swr, const char *text, struct Font *font_bitmaps, uint32_t text_color, int x, int y) {
	swr_crash_if_dest_is_null(swr);

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

		if (swr_draw_glyph(swr, glyph, text_color, x_pos, y_pos)) {
			// Nothing to draw beyond this line.
			// FIXME: Skip to the next line. Also return if we drew the last visible line.
			continue;
		}

		pen_x += glyph.advance_x >> 6;
		pen_y += glyph.advance_y >> 6;
	}
}

float swr_sdf_rect(float x, float y, struct FloatRect rect, float radius) {
	rect.w -= 1.0;
	rect.h -= 1.0;

	x -= rect.x + rect.w / 2.0;
	y -= rect.y + rect.h / 2.0;

	float q_x = fabs(x) - rect.w / 2.0 + radius;
	float q_y = fabs(y) - rect.h / 2.0 + radius;
	float q_x_positive = MAX(0.0, q_x);
	float q_y_positive = MAX(0.0, q_y);

	float result = MIN(0.0, MAX(q_x, q_y)) + sqrt(q_x_positive*q_x_positive + q_y_positive*q_y_positive) - radius;
	return 1.0 - MAX(0.0, MIN(1.0, result));
}

void swr_draw_rectangle_rounded(struct SWRender *swr, struct Rect rect, uint32_t color, float radius) {
	swr_crash_if_dest_is_null(swr);

	struct Rect buffer_rect = {.x = 0, .y = 0, .w = swr->width, .h = swr->height};

	struct Rect visible = swr_rect_intersect(buffer_rect, rect);
	int x_offset = MAX(0, rect.x);
	int y_offset = MAX(0, rect.y);

	struct FloatRect float_rect = {
		.x = rect.x,
		.y = rect.y,
		.w = rect.w,
		.h = rect.h,
	};
	float color_alpha = swr_argb_to_float_alpha(color);

	for (int y = 0; y < visible.h; y++) {
		for (int x = 0; x < visible.w; x++) {
			int sample_x = x + x_offset;
			int sample_y = y + y_offset;

			float alpha = color_alpha * swr_sdf_rect(sample_x, sample_y, float_rect, radius);
			uint32_t the_color = swr_float_alpha_to_argb(alpha) | (color & 0x00FFFFFF);

			int dest_index = sample_y * swr->width + sample_x;
			uint32_t output_color = swr_alpha_blend(swr->dest[dest_index], the_color);
			swr->dest[dest_index] = output_color;
		}
	}
}

float swr_sdf_rect_outline(float x, float y, struct FloatRect rect, float radius, float thickness_inward, float thickness_outward) {
	rect.w -= 1.0;
	rect.h -= 1.0;

	x -= rect.x + rect.w / 2.0;
	y -= rect.y + rect.h / 2.0;

	float q_x = fabs(x) - rect.w / 2.0 + radius;
	float q_y = fabs(y) - rect.h / 2.0 + radius;
	float q_x_positive = MAX(0.0, q_x);
	float q_y_positive = MAX(0.0, q_y);

	float result = MIN(0.0, MAX(q_x, q_y)) + sqrt(q_x_positive*q_x_positive + q_y_positive*q_y_positive) - radius;

	float t_in = thickness_inward / 2.0;
	float t_out = thickness_outward / 2.0;
	result = MAX(0.0, fabs(result - t_out + t_in) - t_out - t_in);
	return 1.0 - MAX(0.0, MIN(1.0, result));
}

void swr_draw_rectangle_rounded_outline(struct SWRender *swr, struct Rect rect, uint32_t color, float radius, float thickness_inward, float thickness_outward) {
	swr_crash_if_dest_is_null(swr);

	struct Rect buffer_rect = {.x = 0, .y = 0, .w = swr->width, .h = swr->height};

	struct Rect visible = swr_rect_intersect(buffer_rect, rect);

	// Need some extra space if the thickness extends outward
	int grow = ceil(thickness_outward);
	visible.x -= grow;
	visible.y -= grow;
	visible.w += 2 * grow;
	visible.h += 2 * grow;
	visible = swr_rect_intersect(visible, buffer_rect);

	int x_offset = MAX(0, rect.x - grow);
	int y_offset = MAX(0, rect.y - grow);

	struct FloatRect float_rect = {
		.x = rect.x,
		.y = rect.y,
		.w = rect.w,
		.h = rect.h,
	};
	float color_alpha = swr_argb_to_float_alpha(color);

	for (int y = 0; y < visible.h; y++) {
		for (int x = 0; x < visible.w; x++) {
			int sample_x = x + x_offset;
			int sample_y = y + y_offset;

			float alpha = color_alpha * swr_sdf_rect_outline(sample_x, sample_y, float_rect, radius, thickness_inward, thickness_outward);
			uint32_t the_color = swr_float_alpha_to_argb(alpha) | (color & 0x00FFFFFF);

			int dest_index = sample_y * swr->width + sample_x;
			uint32_t output_color = swr_alpha_blend(swr->dest[dest_index], the_color);
			swr->dest[dest_index] = output_color;
		}
	}
}
