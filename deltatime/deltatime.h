#pragma once

#include <GLFW/glfw3.h>
#include <time.h>

struct DeltaTimeData {
	double target_fps;
	double delta_sec;

	double last_timestamp;
};

void deltatime_set_target_fps(double target); // Set the target framerate. This decides how long `deltatime_end_frame` will sleep for.
double deltatime_start_frame(); // Returns delta time in seconds
void deltatime_end_frame(); // Sleeps the remaining time to target framerate
double deltatime_fps(); // Returns the current framerate as 1.0 / deltatime_start_frame();
