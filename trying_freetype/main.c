#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <GLFW/glfw3.h>
#include "../sw-render/sw-render.h"

#include "../cvulkan/cvulkan.h"
#include "../sw-render/sw-render.h"

struct UI_Data {
	FT_Library library;
	FT_Error error;
	FT_Face face;
};

#define FONT_W 1000
#define FONT_H 1000

int main() {
	struct UI_Data data;

	data.error = FT_Init_FreeType(&data.library);
	assert(!data.error);

	data.error = FT_New_Face(data.library, "Inter-Regular.ttf", 0, &data.face);
	assert(!data.error);

	//data.error = FT_Set_Char_Size(data.face, 0, 16*64, 300, 300);
	data.error = FT_Set_Pixel_Sizes(data.face, FONT_W, FONT_H);
	assert(!data.error);

	FT_UInt glyph_index = FT_Get_Char_Index(data.face, 'f' /* FT_ULong, UTF-32 */);
	assert(glyph_index);

	data.error = FT_Load_Glyph(data.face, glyph_index, 0 /* https://freetype.org/freetype2/docs/reference/ft2-glyph_retrieval.html#ft_load_xxx */);
	assert(glyph_index);

	data.error = FT_Render_Glyph(data.face->glyph, 0 /* https://freetype.org/freetype2/docs/reference/ft2-glyph_retrieval.html#ft_render_mode */);
	assert(glyph_index);

	printf("left: %i, top: %i\n", data.face->glyph->bitmap_left, data.face->glyph->bitmap_top);
	printf("num: %li\n", data.face->num_glyphs);

	assert(glfwInit());
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	int width = 1280;
	int height = 720;
	GLFWwindow* window = glfwCreateWindow(width, height, "god is dead", NULL, NULL);

	uint32_t *buffer = (uint32_t*)malloc(4 * width * height);
	assert(buffer);
	assert(cvk_init(window));

	int img_width = data.face->glyph->bitmap.width;
	int img_height = data.face->glyph->bitmap.rows;
	uint32_t *img = convert_image_a_to_argb((uint8_t*)data.face->glyph->bitmap.buffer, img_width*img_height);

	printf("buffer: %p, width: %i, height: %i\n", (uint32_t*)data.face->glyph->bitmap.buffer, data.face->glyph->bitmap.width, data.face->glyph->bitmap.rows);

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		usleep(7000);

		memset(buffer, 0, 4*width*height);

		draw_image_argb(buffer, width, height, img, img_width, img_height, 0, 0);

		cvk_draw(window, buffer, width, height);
	}

	cvk_cleanup();
	glfwTerminate();

	return 0;
}
