#include <stdio.h>

#include "../renamethis.h"

int main() {
	FT_Error err = generate_font_atlas("Inter-Regular.ttf", 40);
	if (err) printf("err: %i\n", err);
	return 0;
}
