#include <stdio.h>
#include <unistd.h>

#include "../fontbmp.h"

int main() {
	struct FontBitmaps font_bitmaps = initialize_font_bitmaps();

	FT_Error err = generate_font_bitmaps(&font_bitmaps, "Inter-Regular.ttf", 40);
	if (err) printf("err: %i\n", err);

	for (int c = 0; c <= 255; c++) {
		struct GlyphBitmap glyph = font_bitmaps.glyph_list[c];
		printf("Char %c, index %i, width: %i, height: %i, data: %p\n", (char)c, c, glyph.width, glyph.height, glyph.data);
	}

	// Output the bitmap data
	//write(1, glyph.data, glyph.width*glyph.height);

	deinitialize_font_bitmaps(font_bitmaps);
	return 0;
}
