## deltatime

Deltatime + sleeping for target framerate on GLFW.

Only works on Linux + GLFW for now.

## Usage
```c
deltatime_set_target_fps(60.0);

// Main frame loop
while (1) {
    double delta_sec = deltatime_start_frame();

    // Do work

    deltatime_end_frame();
}
```
