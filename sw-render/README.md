## sw-render.h

A library for software-rendering

The color format is 8-bit ARGB!

## Usage

```c
#include "sw-render.h"

// Initialize the library
SWRender r;
swr_initialize(&r);

// Give it a pointer to your framebuffer, along with its width & height.
swr_set_output(&r, (uint32_t*)framebuffer, width, height);

// Draw something!
while (1) {
    swr_draw_text(
        &r,
        "hello world!",
        swr_rgba(255, 255, 255, 255),
        22,
        0, 0
    );
}

// Deinitialize
swr_deinitialize(&r);
```

## Compilation

Assuming your code is in `main.c`:
You also need the fontbmp library from this c-folder repository...

```
gcc main.c sw-render.c font.c ../fontbmp/fontbmp.c -o main -I/usr/local/include/freetype2 -I/usr/include/libpng16 -I/usr/include/harfbuzz -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include  -lm -lfreetype
```

This produces a binary named `main`

See [tests/compile.sh](tests/compile.sh) for a real example
