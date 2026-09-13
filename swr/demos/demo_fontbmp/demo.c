#include <stdio.h>
#include <unistd.h>

#define SWR_IMPLEMENTATION
#include "../../swr.h"

int main() {
	struct swr_font font = swr_fontbmp_initialize();

	FT_Error err = swr_fontbmp_generate(&font, "Inter-Regular.ttf", 40);
	if (err) printf("err: %i\n", err);

	for (int c = 0; c <= 255; c++) {
		struct swr_glyph_bitmap glyph = font.glyph_list[c];
		printf("Char %c, index %i, width: %i, rows: %i, pitch: %i, data: %p\n", (char)c, c, glyph.width, glyph.rows, glyph.pitch, glyph.bitmap_data);
	}

	swr_fontbmp_deinitialize(font);
	return 0;
}
