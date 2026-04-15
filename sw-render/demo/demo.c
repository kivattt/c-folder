#include <assert.h>
#include <raylib.h>

#include "../sw-render.h"

int main() {
	int image_width = 3840;
	int image_height = 2560;

	struct SWRender r;
	swr_initialize(&r);

	size_t image_size = 4 * image_width * image_height;
	uint32_t *image = malloc(image_size);
	swr_set_output(&r, image, image_width, image_height);

	SetTraceLogLevel(LOG_WARNING);
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);
	InitWindow(1280, 720, "sw-render.c demo");

	while (!WindowShouldClose()) {
		if (IsKeyDown(KEY_Q)) {
			break;
		}

		// Clear the frame buffer
		memset(image, 0, image_size);

		// Draw text using the default font to the frame buffer
		swr_draw_text(&r, "Hello world, from default font!", 22, swr_rgb(255, 255, 255), 75, 75);

		// Draw a rounded rectangle outline around the text
		struct Rect rect = {
			.x = 43,
			.y = 40,
			.w = 400,
			.h = 60,
		};
		swr_draw_rectangle_rounded_outline(&r, rect, swr_rgb(60, 220, 80), 10.0, 1.0, 1.0);

		// Raylib wants an ABGR image, but we output in ARGB.
		// We convert so it looks correct. (Expensive!)
		swr_convert_image_abgr_to_argb(image, image_width * image_height);

		// Do NOT call UnloadImage() on this!
		Image img = {
			.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
			.width = image_width,
			.height = image_height,
			.data = image,
			.mipmaps = 1,
		};

		Texture2D texture = LoadTextureFromImage(img);

		BeginDrawing();
		ClearBackground(BLACK);
		DrawTexture(texture, 0, 0, WHITE); // Draw our software-rendered image
		DrawFPS(5, 5);
		EndDrawing();

		UnloadTexture(texture);
	}

	CloseWindow();
	swr_deinitialize(&r);
}
