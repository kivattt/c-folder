## Usage

```c
#include "fontbmp.h"

struct FontBitmaps font_bitmaps = initialize_font_bitmaps();

FT_Error err = generate_font_bitmaps(&font_bitmaps, "Inter-Regular.ttf", 40);
if (err) { /* ... */ }

for (int c = 0; c <= 255; c++) {
    struct GlyphBitmap glyph = font_bitmaps.glyph_list[c];
    printf("Char %c, index %i, width: %i, height: %i, data: %p\n", (char)c, c, glyph.width, glyph.height, glyph.data);
}

deinitialize_font_bitmaps(font_bitmaps);
```

See [demo/demo.c](demo/demo.c) for a full example

## Build and run the demo

See [../trying\_freetype/README.md](../trying_freetype/README.md) for instructions on installing Freetype2 on your computer
```
cd demo/
./compile.sh
./demo
```

## Checking for memory leaks

```
cd demo/
./compile.sh
./run_memory_checks.sh
```
