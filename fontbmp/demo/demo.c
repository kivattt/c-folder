#include <stdio.h>
#include <unistd.h>

#include "../fontbmp.h"

int main() {
	struct FontBitmaps font_bitmaps = fontbmp_initialize();

	FT_Error err = fontbmp_generate(&font_bitmaps, "Inter-Regular.ttf", 40);
	if (err) printf("err: %i\n", err);

	for (int c = 0; c <= 255; c++) {
		struct GlyphBitmap glyph = font_bitmaps.glyph_list[c];
		printf("Char %c, index %i, width: %i, rows: %i, pitch: %i, data: %p\n", (char)c, c, glyph.width, glyph.rows, glyph.pitch, glyph.data);
	}

	fontbmp_deinitialize(font_bitmaps);
	return 0;
}
