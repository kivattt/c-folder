## c-folder

Everything in this repo is work-in-progress! \
This is my folder of C stuff for fun.

## Projects in this repo

`cvulkan` template for software rendering using Vulkan + GLFW written by [botondmester](https://github.com/botondmester) \
`sw-render` library for software rendering (with buildable demo) \
`fontbmp` library for generating font bitmaps using Freetype2 \
`swayipc` library for receiving Sway `workspace` events via IPC

`taskbar` work-in-progress Linux taskbar for Sway window manager (Wayland) \
`deltatime` terrible library for deltatime+sleep in GLFW frame loop

`trying_freetype` just a folder showing how to compile Freetype \
`drawing_a_fox` program I use to develop new stuff for sw-render

## TODO

I need to use a code formatting tool. And have it warn on weird variable name styles, because currently I'm mixing both camelCase and snake\_case.

I want this:
```c
// Function definition
int function_name(int *my_param);

// User code
int myParam;
int myResult = function_name(&myParam);
```
