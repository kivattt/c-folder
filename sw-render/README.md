## sw-render.h

A library for software-rendering

## Usage

```c

// Initialize the library
SWRender r;
swr_initialize(&r);

// Give it a pointer to your framebuffer, along with its width & height.
swr_set_dest(&r, (uint32_t*)framebuffer, width, height);

// Draw something!
for {
    swr_draw_text(...)
}
```
