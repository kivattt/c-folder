#include <stdio.h>
#include <time.h>

#define SWR_IMPLEMENTATION
#include "../swr.h"

double time_millis() {
	struct timespec time;
	clock_gettime(CLOCK_MONOTONIC, &time);
	//double seconds = (double)time.tv_sec + (double)time.tv_nsec / 1000000000.0;
	double millis = (double)time.tv_sec * 1000.0 + (double)time.tv_nsec / 1000000.0;
	return millis;
}

int main() {
	int n_times;
	double start, duration;

	// swr_fontbmp_generate_from_memory()
	{
		int font_size = 22;
		struct swr_font font = swr_fontbmp_initialize();
		int fd = open("Inter-Regular.ttf", O_RDONLY);
		struct stat st;
		fstat(fd, &st);
		size_t font_data_size = (size_t)st.st_size;
		unsigned char *font_data = mmap(NULL, font_data_size, PROT_READ, MAP_SHARED, fd, 0);
		n_times = 10;
		start = time_millis();
		for (int i = 0; i < n_times; i++) {
			swr_fontbmp_generate_from_memory(&font, font_data, font_data_size, font_size);
		}
		duration = time_millis() - start;
		printf("\x1b[1;30mswr_fontbmp_generate_from_memory (font size: %i) %i times:\x1b[0m %f ms [avg: %f ms]\n", font_size, n_times, duration, duration / (double)n_times);
		close(fd);
		munmap(font_data, font_data_size);
		swr_fontbmp_deinitialize(font);
	}

	// swr_alpha_blend()
	{
		n_times = 100000000;
		uint32_t dest = 0xF0F0F0F0;
		volatile uint32_t result; // To prevent dead code elimination
		start = time_millis();
		for (int i = 0; i < n_times; i++) {
			uint32_t mod = (uint32_t)(i & 0xFF);
			uint32_t src = mod << 24 | mod << 16 | mod << 8 | mod;
			result = swr_alpha_blend(dest, src);
		}
		result += 1; // To prevent -Wunused-but-set-variable warning
		duration = time_millis() - start;
		printf("\x1b[1;30mswr_alpha_blend    %i times:\x1b[0m %f ms [avg: %f ms]\n", n_times, duration, duration / (double)n_times);
	}

	// swr_linear_to_srgb()
	{
		n_times = 100000000;
		volatile float result; // To prevent dead code elimination
		start = time_millis();
		for (int i = 0; i < n_times; i++) {
			result = swr_linear_to_srgb(0.23149876435F /* random value */);
		}
		result += 1.0F; // To prevent -Wunused-but-set-variable warning
		duration = time_millis() - start;
		printf("\x1b[1;30mswr_linear_to_srgb %i times:\x1b[0m %f ms [avg: %f ms]\n", n_times, duration, duration / (double)n_times);
	}

	// swr_draw_fill()
	{
		n_times = 100;
		int width = 3840;
		int height = 2160;
		uint32_t *buffer = malloc((size_t)(width*height * (int)sizeof(uint32_t)));
		struct swr_output r;
		swr_initialize(&r);
		swr_set_output(&r, buffer, width, height);
		volatile uint32_t f_you_compiler = 0; // To prevent the compiler from only running 1 iteration of the for-loop

		uint32_t color = swr_rgb(18, 18, 20);
		start = time_millis();
		for (int i = 0; i < n_times; i++) {
			swr_draw_fill(&r, color);
			f_you_compiler += buffer[0];
		}
		duration = time_millis() - start;

		swr_deinitialize(&r);

		free(buffer);
		printf("\x1b[1;30mswr_draw_fill() (res: %i x %i) %i times:\x1b[0m %f ms [avg: %f ms]\n", width, height, n_times, duration, duration / (double)n_times);
	}

	// swr_abgr_to_argb()
	{
		n_times = 100000000;
		volatile uint32_t abgr = 0x10203040; // To prevent dead code elimination
		volatile uint32_t result; // To prevent dead code elimination
		start = time_millis();
		for (int i = 0; i < n_times; i++) {
			result = swr_abgr_to_argb(abgr);
		}
		result += 1; // To prevent -Wunused-but-set-variable warning
		duration = time_millis() - start;
		printf("\x1b[1;30mswr_abgr_to_argb()        %i times:\x1b[0m %f ms [avg: %f ms]\n", n_times, duration, duration / (double)n_times);
	}

	// swr_argb_to_float_alpha()
	{
		n_times = 100000000;
		volatile uint32_t argb = 0x10203040; // To prevent dead code elimination
		volatile float result; // To prevent dead code elimination
		start = time_millis();
		for (int i = 0; i < n_times; i++) {
			result = swr_argb_to_float_alpha(argb);
		}
		result += 1.0F; // To prevent -Wunused-but-set-variable warning
		duration = time_millis() - start;
		printf("\x1b[1;30mswr_argb_to_float_alpha() %i times:\x1b[0m %f ms [avg: %f ms]\n", n_times, duration, duration / (double)n_times);
	}

	// swr_float_alpha_to_argb()
	{
		n_times = 100000000;
		volatile float alpha = 0.12379824F; // To prevent dead code elimination
		volatile uint32_t result; // To prevent dead code elimination
		start = time_millis();
		for (int i = 0; i < n_times; i++) {
			result = swr_float_alpha_to_argb(alpha);
		}
		result += 1; // To prevent -Wunused-but-set-variable warning
		duration = time_millis() - start;
		printf("\x1b[1;30mswr_float_alpha_to_argb() %i times:\x1b[0m %f ms [avg: %f ms]\n", n_times, duration, duration / (double)n_times);
	}
}
