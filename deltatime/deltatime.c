#include "deltatime.h"

struct DeltaTimeData deltatime_data_global;

void deltatime_set_target_fps(double target) {
	deltatime_data_global.target_fps = target;
}

double deltatime_start_frame() {
	deltatime_data_global.last_timestamp = glfwGetTime();
	return deltatime_data_global.delta_sec;
}

void deltatime_end_frame() {
	double duration = glfwGetTime() - deltatime_data_global.last_timestamp;
	double sleep_sec = 1.0 / deltatime_data_global.target_fps - duration;
	if (sleep_sec < 0) {
		sleep_sec = 0.0;
	}

	struct timespec req = {0};
	time_t sec = sleep_sec;
	req.tv_sec = sec;
	req.tv_nsec = (sleep_sec - sec) * 1000000000L;
	nanosleep(&req, &req);

	deltatime_data_global.delta_sec = glfwGetTime() - deltatime_data_global.last_timestamp;
}

double deltatime_fps() {
	return 1.0 / deltatime_data_global.delta_sec;
}
