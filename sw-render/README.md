## sw-render.h

A library for software-rendering

## Usage

```c
SWRender r;

swr_initialize(&r);
swr_set_buffer(&r, framebuffer, width, height);

for {
    swr_draw_text(...)
}
```
