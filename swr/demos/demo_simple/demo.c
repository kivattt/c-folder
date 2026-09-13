#include <stdio.h>

#define SWR_IMPLEMENTATION
#include "../../swr.h"

int main() {
	struct swr_output swr;
	swr_initialize(&swr);
	swr_deinitialize(&swr);
}
