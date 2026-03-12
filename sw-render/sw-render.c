//#undef NDEBUG
#include <assert.h>
#include <stdint.h>
#include <immintrin.h>

#ifdef __clang__
	#define ROTR(a, b) __builtin_rotateright32((a), (b))
#else
	#define ROTR(a, b) _rotr((a), (b))
#endif

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

struct Rect {
	int32_t x, y, w, h;
};

// Returns the intersection of two rectangles.
// If there is no intersection, it returns an empty rectangle with x=0, y=0, w=0, h=0
struct Rect rect_intersect(struct Rect a, struct Rect b) {
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

uint32_t abgr_to_argb(uint32_t abgr) {
	return ROTR(__builtin_bswap32(abgr), 8);
}

uint32_t fade_opacity_to_black(uint32_t argb) {
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
void draw_image_abgr(uint32_t *buffer, int buffer_width, int buffer_height, uint32_t* img, int img_width, int img_height, int img_x, int img_y) {
	struct Rect buffer_rect = {.x = 0,     .y = 0,     .w = buffer_width, .h = buffer_height};
	struct Rect img_rect =    {.x = img_x, .y = img_y, .w = img_width,    .h = img_height};

	struct Rect visible = rect_intersect(buffer_rect, img_rect);
	assert(visible.x >= 0);
	assert(visible.y >= 0);
	assert(visible.w >= 0);
	assert(visible.h >= 0);

	int x_offset = 0, y_offset = 0;
	if (img_x < 0) x_offset = -img_x;
	if (img_y < 0) y_offset = -img_y;

	for (int y = 0; y < visible.h; y++) {
		for (int x = 0; x < visible.w; x++) {
			int img_sample_x = x + x_offset;
			int img_sample_y = y + y_offset;
			int img_index = img_sample_y * img_width + img_sample_x;
			uint32_t img_color = fade_opacity_to_black(abgr_to_argb(img[img_index])); // Slow memory fetch bottleneck

			int buffer_index = (visible.y+y) * buffer_width + (visible.x+x);
			buffer[buffer_index] = img_color;
		}
	}
}

// TODO:
// Make a font texture atlas generator tool with SFML (which uses freetype)
/*void draw_text(uint32_t *buffer, int buffer_width, int buffer_height, char *text, int font_size, uint32_t color_argb) {
	
}*/
