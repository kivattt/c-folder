#include <stdio.h>
#include <unistd.h>

#include "../renamethis.h"

int main() {
	struct GlyphBitmap *glyph_bitmap_list;
	uint8_t *bitmap_data;

	FT_Error err = generate_font_atlas(&glyph_bitmap_list, &bitmap_data, "Inter-Regular.ttf", 40);
	if (err) printf("err: %i\n", err);

	struct GlyphBitmap glyph = glyph_bitmap_list['f'];
	printf("width: %i, height: %i, data: %p\n", glyph.width, glyph.height, glyph.data);

	// Output the bitmap data
	//write(1, glyph.data, glyph.width*glyph.height);

	free(glyph_bitmap_list);
	free(bitmap_data);
	return 0;
}
