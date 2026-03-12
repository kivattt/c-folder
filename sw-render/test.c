#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "sw-render.h"

struct TestCaseRectIntersect {
	struct Rect a;
	struct Rect b;
	struct Rect expected;
};

struct TestCaseRectIntersect helper(int32_t x1, int32_t y1, int32_t w1, int32_t h1, int32_t x2, int32_t y2, int32_t w2, int32_t h2, int32_t x_expected, int32_t y_expected, int32_t w_expected, int32_t h_expected) {
	struct Rect a;
	a.x = x1;
	a.y = y1;
	a.w = w1;
	a.h = h1;

	struct Rect b;
	b.x = x2;
	b.y = y2;
	b.w = w2;
	b.h = h2;

	struct Rect expected;
	expected.x = x_expected;
	expected.y = y_expected;
	expected.w = w_expected;
	expected.h = h_expected;

	struct TestCaseRectIntersect out;
	out.a = a;
	out.b = b;
	out.expected = expected;
	return out;
}

void rect_print(struct Rect r) {
	printf("x: %i, y: %i, w: %i, h: %i\n", r.x, r.y, r.w, r.h);
}

int test_rect_intersect() {
#define NUM_INTERSECT_TESTS 4
	struct TestCaseRectIntersect tests[NUM_INTERSECT_TESTS] = {
		helper(0, 0, 100, 100,
		       50, 50, 100, 100,
		       50, 50, 50, 50),
		helper(50, 50, 100, 100,
		       0, 0, 100, 100,
		       50, 50, 50, 50),

		helper(0, 0, 100, 100,
		       30, 80, 40, 100,
		       30, 80, 40, 20),

		helper(0, 0, 10, 10,
		       20, 20, 10, 10,
		       0, 0, 0, 0)
	};

	for (int i = 0; i < NUM_INTERSECT_TESTS; i++) {
		struct Rect result = rect_intersect(tests[i].a, tests[i].b);
		if (memcmp(&result, &tests[i].expected, sizeof(struct Rect)) != 0) {
			printf("Test at index %i failed!\n\n", i);

			printf("Expected:\n");
			rect_print(tests[i].expected);
			printf("But got:\n");
			rect_print(result);
			return 1;
		}
	}

	return 0;
}

int main() {
	int rect_intersect_result = test_rect_intersect();
	if (!rect_intersect_result) return 1;
}
