#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <immintrin.h>

#include <GLFW/glfw3.h>

#include "cvulkan.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

int running = 1;
int x = 0;
int y = 0;

int key_w = 0;
int key_a = 0;
int key_s = 0;
int key_d = 0;

void handle_input(GLFWwindow *window, int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_Q) {
		running = 0;
	}

	if (key == GLFW_KEY_W) {
		if (action == GLFW_PRESS) key_w = 1;
		else if (action == GLFW_RELEASE) key_w = 0;
	}
	if (key == GLFW_KEY_A) {
		if (action == GLFW_PRESS) key_a = 1;
		else if (action == GLFW_RELEASE) key_a = 0;
	}
	if (key == GLFW_KEY_S) {
		if (action == GLFW_PRESS) key_s = 1;
		else if (action == GLFW_RELEASE) key_s = 0;
	}
	if (key == GLFW_KEY_D) {
		if (action == GLFW_PRESS) key_d = 1;
		else if (action == GLFW_RELEASE) key_d = 0;
	}
}

static uint32_t abgr_to_argb(uint32_t abgr) {
	//return (abgr & 0xFF000000) | (_bswap(abgr) >> 8);
	return _rotr(_bswap(abgr), 8);
}

uint32_t fade_opacity_to_black(uint32_t argb) {
	uint8_t alpha = argb >> 24; // 0 to 255
	uint16_t r = (argb >> 16) & 0xff;
	uint16_t g = (argb >> 8) & 0xff;
	uint16_t b = (argb >> 0) & 0xff;

	r *= alpha;
	g *= alpha;
	b *= alpha;

	r /= 255;
	g /= 255;
	b /= 255;

	return 0xFF000000 | r << 16 | g << 8 | b;
}

struct ARGB {
	uint8_t a;
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

#define COLOR_MAX 255

/*uint32_t blend(struct ARGB src, struct ARGB dst) {
//	float outAlpha = ((struct ARGB*)&src)->a;
	float outAlpha = src.a + dst.a * (COLOR_MAX - src.a);
	uint32_t outColor = ((*(uint32_t*)&src) & 0x00FFFFFF) * src.a;
}*/

void draw_guy(void* image, int width, int height, uint32_t* guyImage, int guyWidth, int guyHeight, int x, int y) {
	// TODO: Compute overlapping visible rectangle and draw inside that
	for (int i = 0; i < guyWidth*guyHeight; i++) {
		int imageX = x + i % guyWidth;
		int imageY = y + i / guyWidth;
		int imageIndex = imageY * width + imageX;
		if (imageX < 0 || imageX >= width) {
			return;
		}
		if (imageY < 0 || imageY >= height) {
			return;
		}

		uint32_t guyColor = fade_opacity_to_black(abgr_to_argb(guyImage[i]));
		((uint32_t*)image)[imageIndex] = guyColor;
	}
}

int main() {
	int width = 1280;
	int height = 720;
	assert(glfwInit());

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	GLFWwindow* window = glfwCreateWindow(width, height, "GLFW+Vulkan for software rendering", NULL, NULL);

	int guyWidth, guyHeight, guyChannels;
	unsigned char *guyImage = stbi_load("fox.png", &guyWidth, &guyHeight, &guyChannels, 0);
	printf("guy channels: %i\n", guyChannels);

	int bufferWidth = width;
	int bufferHeight = height;
	//x=bufferWidth/2, y=bufferHeight/2;
	void* image = malloc(4 * bufferWidth * bufferHeight);
	assert(image);

	assert(cvk_init(window));

	glfwSetKeyCallback(window, handle_input);

	while (!glfwWindowShouldClose(window) && running) {
		glfwPollEvents();
		int speed = 5;
		if (key_w) y -= speed;
		if (key_s) y += speed;
		if (key_a) x -= speed;
		if (key_d) x += speed;
		usleep(7000); // 7 ms

		//for (int i = 0; i < width*height; i++) {
		/*int drawWidth = MIN(width, bufferWidth);
		int drawHeight = MIN(height, bufferHeight);
		for (int i = 0; i < drawWidth*drawHeight; i++) {
			// ARGB
			//((uint32_t*)image)[i] = 0xFF00FF00;
			int x = i % drawWidth;
			int y = i / drawWidth;
			float sauce = 0.00025 * (x*x+y*y);
			int value = sauce;
			int val = value & 0xff;
		
			((uint32_t*)image)[i] = 0xFF000000 | val << 16 | val << 8 | val;
		}*/

		memset(image, 0, 4 * bufferWidth * bufferHeight);

		//draw_guy(image, width, height, guyImage, guyWidth, guyHeight, x ,y);
		draw_guy(image, bufferWidth, bufferHeight, (uint32_t*)guyImage, guyWidth, guyHeight, x ,y);

		cvk_draw(window, image, width, height);
	}

	cvk_cleanup();

	glfwTerminate();

	stbi_image_free(guyImage);

	return 0;
}
