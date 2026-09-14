#include <stdio.h>
#include <time.h>

#define SWR_IMPLEMENTATION
#include "../swr.h"

double time_sec() {
	struct timespec time;
	clock_gettime(CLOCK_MONOTONIC, &time);
	double time_sec = time.tv_sec + time.tv_nsec / 1000000000.0;
	return time_sec;
}

int main() {
	// Font bitmap functions
	struct swr_font font = swr_fontbmp_initialize();
	int fd = open("Inter-Regular.ttf", O_RDONLY);
	struct stat st;
	fstat(fd, &st);
	size_t font_data_size = (size_t)st.st_size;
	unsigned char *font_data = mmap(NULL, font_data_size, PROT_READ, MAP_SHARED, fd, 0);

	double start = time_sec();
	int n_times = 100;
	for (int i = 0; i < n_times; i++) {
		swr_fontbmp_generate_from_memory(&font, font_data, font_data_size, 22);
	}
	double duration = time_sec() - start;
	printf("swr_fontbmp_generate_from_memory %i times: %fs\n", n_times, duration);

	close(fd);
	munmap(font_data, font_data_size);
	swr_fontbmp_deinitialize(font);

	// ...
}
