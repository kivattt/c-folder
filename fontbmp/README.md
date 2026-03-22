## fontbmp

This library creates an array of bitmap data that contains the characters 0 through 255 from a font file. \
Intended to be used for software rendering text.

You can re-run `generate_font_bitmaps` with a different font-size to change the font size of all the bitmaps.

This library uses freetype to render the glyphs.

It contains only 3 functions:
```
initialize_font_bitmaps
deinitialize_font_bitmaps
generate_font_bitmaps
```

See [fontbmp.h](fontbmp.h) for detailed definitions

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

## Known issues

- The `font_height_pixels` parameter in `generate_font_bitmaps` doesn't actually let you choose the pixel height, it can go above or below.

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
