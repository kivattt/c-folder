#ifndef SWR_H
#define SWR_H

struct SWRender {
	uint32_t *dest; // Destination image (8-bit ARGB)
	int width;
	int height;
};

#define SWR_IMPLEMENTATION // DEV
#ifdef SWR_IMPLEMENTATION

void swr_initialize()

#endif // SWR_IMPLEMENTATION

#endif // SWR_H
