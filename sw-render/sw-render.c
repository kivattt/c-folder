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
	if (swr == NULL) {
		printf("sw-render: swr_initialize called with a NULL pointer. Remember: your SWRender struct should be on the stack!\n");
		assert(0);
	}

	memset(swr, 0, sizeof(struct SWRender));
	swr->last_default_font_size = -1;

	swr->default_font = fontbmp_initialize();
}

void swr_deinitialize(struct SWRender *swr) {
	fontbmp_deinitialize(swr->default_font);
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

int swr_is_point_in_rect(struct Rect rect, int x, int y) {
	if (rect.w == 0 && rect.h == 0) {
		return 0;
	}

	int isXWithin = x >= rect.x && x <= (rect.x+rect.w);
	int isYWithin = y >= rect.y && y <= (rect.y+rect.h);
	return isXWithin && isYWithin;
}

uint32_t swr_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	return a << 24 | r << 16 | g << 8 | b;
}

uint32_t swr_rgb(uint8_t r, uint8_t g, uint8_t b) {
	return swr_rgba(r, g, b, 0xff);
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
	// Fast path
	if (src >> 24 == 0xff) {
		return src;
	}

	dest |= 0xFF000000;

	uint8_t a = src >> 24;
	uint8_t r = (((src >> 16) & 0xFF) * a) / 255 + (((dest >> 16) & 0xFF) * (255 - a)) / 255;
	uint8_t g = (((src >>  8) & 0xFF) * a) / 255 + (((dest >>  8) & 0xFF) * (255 - a)) / 255;
	uint8_t b = (((src >>  0) & 0xFF) * a) / 255 + (((dest >>  0) & 0xFF) * (255 - a)) / 255;

	return 0xFF000000 | r << 16 | g << 8 | b;
}

uint32_t swr_color_tint(uint32_t color, uint32_t tint) {
	// Fast path
	if (tint == 0xFFFFFFFF) {
		return color;
	}

	uint8_t a = ((color >> 24) & 0xff) * ((tint >> 24) & 0xff) / 255;
	uint8_t r = ((color >> 16) & 0xff) * ((tint >> 16) & 0xff) / 255;
	uint8_t g = ((color >>  8) & 0xff) * ((tint >>  8) & 0xff) / 255;
	uint8_t b = ((color >>  0) & 0xff) * ((tint >>  0) & 0xff) / 255;

	return a << 24 | r << 16 | g << 8 | b;
}

// Converts an image from ABGR to ARGB in-place
void swr_convert_image_abgr_to_argb(uint32_t *img, int length) {
	for (int i = 0; i < length; i++) {
		img[i] = swr_abgr_to_argb(img[i]);
	}
}

// Converts an image from ARGB to ABGR in-place
void swr_convert_image_argb_to_abgr(uint32_t *img, int length) {
	// The conversion is two-way symmetrical
	return swr_convert_image_abgr_to_argb(img, length);
}

void swr_draw_fill_background(struct SWRender *swr, uint8_t red, uint8_t green, uint8_t blue) {
	// A background with transparency behaves strangely, so force it to be fully opaque (255).
	uint32_t color = swr_rgba(red, green, blue, 255);

	for (int y = 0; y < swr->height; y++) {
		for (int x = 0; x < swr->width; x++) {
			int index = y * swr->width + x;
			swr->dest[index] = color;
		}
	}
}

void swr_draw_image(struct SWRender *swr, uint32_t *img_argb, int width, int height, int x, int y) {
	swr_draw_image_ex(swr, img_argb, width, height, 0xFFFFFFFF, 1.0, x, y);
}

void swr_draw_image_ex(struct SWRender *swr, uint32_t *img_argb, int width, int height, uint32_t color_tint, float scale, int x, int y) {
	swr_crash_if_dest_is_null(swr);

	if (width <= 0 || height <= 0) {
		return;
	}

	if (scale == 1.0) {
		struct Rect buffer_rect = {.x = 0, .y = 0, .w = swr->width, .h = swr->height};
		struct Rect img_rect =    {.x = x, .y = y, .w = width,      .h = height};

		struct Rect visible = swr_rect_intersect(buffer_rect, img_rect);
		int x_offset = -MIN(0, x);
		int y_offset = -MIN(0, y);

		for (int dy = 0; dy < visible.h; dy++) {
			int img_sample_x = x_offset;
			int img_sample_y = dy + y_offset;
			int img_index = img_sample_y * width + img_sample_x;

			int dest_index = (visible.y+dy) * swr->width + visible.x;

			for (int dx = 0; dx < visible.w; dx++) {
				// Sample the image pixel
				uint32_t img_color = swr_color_tint(img_argb[img_index + dx], color_tint);

				// Set the output pixel, with alpha-blending
				swr->dest[dest_index + dx] = swr_alpha_blend(swr->dest[dest_index + dx], img_color);
			}
		}
	} else {
		scale = MAX(0.0, scale);
		int width_scaled = width * scale;
		int height_scaled = height * scale;

		// To guard against a divide-by-zero later.
		if (width_scaled <= 1 || height_scaled <= 1) {
			return;
		}

		struct Rect buffer_rect = {.x = 0, .y = 0, .w = swr->width,    .h = swr->height};
		struct Rect img_rect =    {.x = x, .y = y, .w = width_scaled, .h = height_scaled};

		struct Rect visible = swr_rect_intersect(buffer_rect, img_rect);

		// FIXME: use these variables to correctly render when x < 0 or y < 0. We may have to divide these by scale.
		//int x_offset = -MIN(0, x);
		//int y_offset = -MIN(0, y);

		for (int dy = 0; dy < visible.h; dy++) {
			for (int dx = 0; dx < visible.w; dx++) {
				int img_sample_x = (float)dx / (float)(width_scaled - 1) * (width - 1);
				int img_sample_y = (float)dy / (float)(height_scaled - 1) * (height - 1);
				assert(img_sample_x >= 0 && img_sample_y >= 0);
				assert(img_sample_x < width && img_sample_y < height);

				// Sample the image pixel
				int img_index = img_sample_y * width + img_sample_x;
				uint32_t img_color = swr_color_tint(img_argb[img_index], color_tint);

				// Set the output pixel, with alpha-blending
				int dest_index = (visible.y + dy) * swr->width + (visible.x + dx);
				swr->dest[dest_index] = swr_alpha_blend(swr->dest[dest_index], img_color);
			}
		}
	}
}

// Draws a single-channel 8-bit alpha image to dest, outputs in ARGB
// Returns non-zero if it would draw outside of the screen (nothing to draw)
int swr_draw_glyph(struct SWRender *swr, struct FontBMPGlyphBitmap img, uint32_t color, int img_x, int img_y) {
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

struct Rect swr_draw_text(struct SWRender *swr, const char *text, uint32_t size, uint32_t color, int x, int y) {
	if (size != swr->last_default_font_size) {
		int err = fontbmp_generate_from_memory(&swr->default_font, swr_default_font_data, swr_default_font_data_size, size);
		if (err) {
			printf("swr_draw_text: A call to fontbmp_generate_from_memory errored with code %i\n", err);
			assert(0);
		}

		swr->last_default_font_size = size;
	}

	return swr_draw_text_ex(swr, text, &swr->default_font, color, x, y);
}

struct Rect swr_draw_text_ex(struct SWRender *swr, const char *text, struct FontBMPFont *font_bitmaps, uint32_t color, int x, int y) {
	return swr_draw_text_impl(swr, text, font_bitmaps, color, x, y, 1 /* Yes, draw to the screen */);
}

struct Rect swr_measure_text_ex(struct SWRender *swr, const char *text, struct FontBMPFont *font_bitmaps, uint32_t color, int x, int y) {
	return swr_draw_text_impl(swr, text, font_bitmaps, color, x, y, 0 /* Don't draw anything */);
}

// Returns text width and height, along with xy offset from the original x,y inputs.
struct Rect swr_draw_text_impl(struct SWRender *swr, const char *text, struct FontBMPFont *font_bitmaps, uint32_t color, int x, int y, int actually_draw) {
	swr_crash_if_dest_is_null(swr);

	int pen_x = x;
	int pen_y = y + (font_bitmaps->ascender >> 6);

	struct Rect bounding_box = {
		.x = 2147483647,
		.y = 2147483647,
		.w = 0,
		.h = 0,
	};

	for (int i = 0; i < strlen(text); i++) {
		unsigned char c = *(unsigned char*)&text[i]; // Need to reinterpret signed char as unsigned.

		struct FontBMPGlyphBitmap glyph = font_bitmaps->glyph_list[c];
		if (glyph.bitmap_data == NULL) {
			pen_x += glyph.advance_x >> 6;
			pen_y += glyph.advance_y >> 6;

			if (c == '\n') {
				pen_x = 0;
			}

			// TODO: Add bounding_box growth here aswell
			continue;
		}

		int x_pos = pen_x + glyph.bitmap_left;
		int y_pos = pen_y - glyph.bitmap_top;

		if (actually_draw) {
			if (swr_draw_glyph(swr, glyph, color, x_pos, y_pos)) {
				// Nothing to draw beyond this line.
				// FIXME: Skip to the next line. Also return if we drew the last visible line.
				continue;
			}
		}

		// Bounding box growth
		int min_x = x_pos;
		int min_y = y_pos;
		int max_x = x_pos + glyph.width;
		int max_y = y_pos + glyph.rows;
		if (min_x < bounding_box.x) bounding_box.x = min_x;
		if (min_y < bounding_box.y) bounding_box.y = min_y;
		if (max_x > bounding_box.w) bounding_box.w = max_x;
		if (max_y > bounding_box.h) bounding_box.h = max_y;

		pen_x += glyph.advance_x >> 6;
		pen_y += glyph.advance_y >> 6;
	}

	bounding_box.w -= bounding_box.x;
	bounding_box.h -= bounding_box.y;
	bounding_box.x -= x;
	bounding_box.y -= y;
	return bounding_box;
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

void swr_draw_rectangle(struct SWRender *swr, struct Rect rect, uint32_t color) {
	swr_crash_if_dest_is_null(swr);

	struct Rect buffer_rect = {.x = 0, .y = 0, .w = swr->width, .h = swr->height};

	struct Rect visible = swr_rect_intersect(buffer_rect, rect);
	int x_offset = MAX(0, rect.x);
	int y_offset = MAX(0, rect.y);

	for (int y = 0; y < visible.h; y++) {
		for (int x = 0; x < visible.w; x++) {
			int out_x = x + x_offset;
			int out_y = y + y_offset;

			int dest_index = out_y * swr->width + out_x;
			uint32_t output_color = swr_alpha_blend(swr->dest[dest_index], color);
			swr->dest[dest_index] = output_color;
		}
	}
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
