## deltatime

**Only meant to be used with the ../cvulkan library**

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

## Known issues

- If the target fps is higher than whatever cvulkan chose, the deltatime will be wrong (smaller) when moving the window around
- We're probably sleeping in the wrong place, should probably be before cvk\_draw...
