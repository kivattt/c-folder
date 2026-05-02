#define NDEBUG
#include <assert.h>

#include <raylib.h>
#include "../taskbar.h"
#include "../../sw-render/sw-render.h"

int main() {
	int image_width = 3840;
	int image_height = 2560;

	size_t image_size = 4 * image_width * image_height;
	uint32_t *image = malloc(image_size);

	SetTraceLogLevel(LOG_WARNING);
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);
	InitWindow(1920, 30, "taskbar demo");
	SetWindowMinSize(0, 0);
	SetWindowMaxSize(image_width, image_height);

	struct Taskbar tb;
	int err = taskbar_initialize(&tb, "../assets");
	if (err) {
		return err;
	}
	tb.debug = 1;

	while (!WindowShouldClose()) {
		if (IsKeyDown(KEY_Q)) {
			break;
		}

		int window_width = GetScreenWidth();
		int window_height = GetScreenHeight();

		// Clear the frame buffer
		memset(image, 0, image_size);

		int bar_height = 30;
		taskbar_draw(&tb, 0, NULL /* monitor_name */, image, window_width, window_height, bar_height);

		// SLOW: Raylib expects RGBA, while taskbar_draw uses ARGB.
		swr_convert_image_argb_to_abgr(image, image_width*image_height);

		// Do NOT call UnloadImage() on this!
		Image img = {
			.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
			.width = window_width,
			.height = window_height,
			.data = image,
			.mipmaps = 1,
		};

		Texture2D texture = LoadTextureFromImage(img);

		BeginDrawing();
		ClearBackground(BLACK);
		DrawTexture(texture, 0, 0, WHITE); // Draw our software-rendered image
		if (tb.debug) {
			DrawFPS(GetScreenWidth() - 600, 5);
		}
		EndDrawing();

		UnloadTexture(texture);
	}

	CloseWindow();
	taskbar_deinitialize(&tb);
}
