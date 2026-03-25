#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <immintrin.h>

#include <GLFW/glfw3.h>

#include "../cvulkan/cvulkan.h"
#include "../sw-render/sw-render.h"
#include "../fontbmp/fontbmp.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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

int main() {
	int width = 1280;
	int height = 720;
	assert(glfwInit());

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	GLFWwindow* window = glfwCreateWindow(width, height, "GLFW+Vulkan for software rendering", NULL, NULL);

	int guyWidth, guyHeight, guyChannels;
	unsigned char *guyImage = stbi_load("fox.png", &guyWidth, &guyHeight, &guyChannels, 4);
	printf("guy channels: %i\n", guyChannels);

	swr_convert_image_abgr_to_argb((uint32_t*)guyImage, guyWidth*guyHeight);

	int bufferWidth = width;
	int bufferHeight = height;
	void* buffer = malloc(4 * bufferWidth * bufferHeight);
	assert(buffer);

	assert(cvk_init(window));

	glfwSetKeyCallback(window, handle_input);

	struct Font font = fontbmp_initialize();

	int font_size = 40;
	char *font_filename = "Inter-Regular.ttf";
	//char *font_filename = "JetBrainsMono-Regular.ttf";
	FT_Error err = fontbmp_generate(&font, font_filename, font_size);
	if (err) {
		printf("Error loading font: %i\n", err);
		return 1;
	}

	//char thing[256];
	char *thing = malloc(255);
	for (int i = 1; i < 256; i++) {
		thing[i-1] = i;
	}

	int i = 0;
	while (!glfwWindowShouldClose(window) && running) {
		++i;
		if (0 && (i % 4 == 0)) {
			font_size += 1;
			//printf("gen...\n");
			FT_Error err = fontbmp_generate(&font, font_filename, font_size);
			if (err) {
				printf("Error loading font: %i\n", err);
				return 1;
			}
		}

		glfwPollEvents();
		int speed = 9;
		if (key_w) y -= speed;
		if (key_s) y += speed;
		if (key_a) x -= speed;
		if (key_d) x += speed;
		usleep(7000); // 7 ms

		// Clear the screen
		memset(buffer, 0, 4 * bufferWidth * bufferHeight);

		swr_draw_image_argb(buffer, bufferWidth, bufferHeight, (uint32_t*)guyImage, guyWidth, guyHeight, x, y);
		swr_draw_text(buffer, bufferWidth, bufferHeight, "s\xe5nn er det bare. \xe6\xf8\xe5\nhello world!\nline 2\nline 3\n", &font, 200, 200);

		cvk_draw(window, buffer, width, height);
	}

	cvk_cleanup();

	glfwTerminate();

	stbi_image_free(guyImage);

	fontbmp_deinitialize(font);

	return 0;
}
