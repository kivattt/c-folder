#include <stdio.h>
#include <unistd.h>

#include "../fontbmp.h"

int main() {
	struct Font font = fontbmp_initialize();

	FT_Error err = fontbmp_generate(&font, "Inter-Regular.ttf", 40);
	if (err) printf("err: %i\n", err);

	for (int c = 0; c <= 255; c++) {
		struct GlyphBitmap glyph = font.glyph_list[c];
		printf("Char %c, index %i, width: %i, rows: %i, pitch: %i, data: %p\n", (char)c, c, glyph.width, glyph.rows, glyph.pitch, glyph.bitmap_data);
	}

	fontbmp_deinitialize(font);
	return 0;
}
