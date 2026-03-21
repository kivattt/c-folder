#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "../sw-render.h"

void print_gray(const char *s) {
	printf("\x1b[1;30m%s\x1b[0m", s);
}

void print_red(const char *s) {
	printf("\x1b[31m%s\x1b[0m", s);
}

void print_green(const char *s) {
	printf("\x1b[32m%s\x1b[0m", s);
}

void rect_print(struct Rect r) {
	printf("x: %i, y: %i, w: %i, h: %i\n", r.x, r.y, r.w, r.h);
}

struct TestCaseRectIntersect {
	struct Rect a;
	struct Rect b;
	struct Rect expected;
};

int test_rect_intersect() {
#define NUM_INTERSECT_TESTS 4
	struct TestCaseRectIntersect tests[NUM_INTERSECT_TESTS] = {
		{
			.a = {0, 0, 100, 100},
			.b = {50, 50, 100, 100},
			.expected = {50, 50, 50, 50},
		},

		{
			.a = {50, 50, 100, 100},
			.b = {0, 0, 100, 100},
			.expected = {50, 50, 50, 50},
		},

		{
			.a = {0, 0, 100, 100},
			.b = {30, 80, 40, 100},
			.expected = {30, 80, 40, 20},
		},

		{
			.a = {0, 0, 10, 10},
			.b = {20, 20, 10, 10},
			.expected = {0, 0, 0, 0},
		},
	};

	for (int i = 0; i < NUM_INTERSECT_TESTS; i++) {
		struct Rect result = rect_intersect(tests[i].a, tests[i].b);
		if (memcmp(&result, &tests[i].expected, sizeof(struct Rect)) != 0) {
			print_red(" Failed\n");

			printf("Test at index %i failed!\n\n", i);

			printf("Expected:\n");
			rect_print(tests[i].expected);
			printf("But got:\n");
			rect_print(result);
			return 1;
		}
	}

	print_green(" Success\n");
	return 0;
}

int test_abgr_to_argb() {
	uint32_t input =    0xFFAABBCC;
	uint32_t expected = 0xFFCCBBAA;
	uint32_t result = abgr_to_argb(input);
	if (result != expected) {
		print_red(" Failed\n");

		printf("Expected: %x\n", expected);
		printf("But got : %x\n", result);
		return 1;
	}

	print_green(" Success\n");
	return 0;
}

int main() {
	int result;
	int failed = 0;

	print_gray("test_rect_intersect():");
	result = test_rect_intersect();
	if (result != 0) {
		failed = 1;
	}

	print_gray("test_abgr_to_argb():");
	result = test_abgr_to_argb();
	if (result != 0) {
		failed = 1;
	}

	printf("\n");
	if (failed) {
		print_red("FAIL\n");
		return 1;
	} else {
		print_green("PASS\n");
		return 0;
	}
}
