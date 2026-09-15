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
	int n_times;
	double start, duration;

	// swr_fontbmp_generate_from_memory()
	struct swr_font font = swr_fontbmp_initialize();
	int fd = open("Inter-Regular.ttf", O_RDONLY);
	struct stat st;
	fstat(fd, &st);
	size_t font_data_size = (size_t)st.st_size;
	unsigned char *font_data = mmap(NULL, font_data_size, PROT_READ, MAP_SHARED, fd, 0);

	start = time_sec();
	n_times = 10;
	for (int i = 0; i < n_times; i++) {
		swr_fontbmp_generate_from_memory(&font, font_data, font_data_size, 22);
	}
	duration = time_sec() - start;
	printf("swr_fontbmp_generate_from_memory %i times: %fs\n", n_times, duration);

	close(fd);
	munmap(font_data, font_data_size);
	swr_fontbmp_deinitialize(font);

	// swr_alpha_blend()
	start = time_sec();
	n_times = 100000000;
	uint32_t dest = 0xF0F0F0F0;
	volatile uint32_t result; // To prevent dead code elimination
	for (int i = 0; i < n_times; i++) {
		uint8_t mod = i & 0xFF;
		uint32_t src = mod << 24 | mod << 16 | mod << 8 | mod;
		result = swr_alpha_blend(dest, src);
	}
	duration = time_sec() - start;
	printf("swr_alpha_blend %i times: %fs\n", n_times, duration);
}
