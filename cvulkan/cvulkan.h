#pragma once

#include <GLFW/glfw3.h>

#include <stdbool.h>

// note: the caller is responsible for preserving the glfw window resize callback if they set one after initialization
//       as cvk relies on the window resize callback
bool cvk_init(GLFWwindow* window);

void cvk_cleanup(void);

void cvk_draw(GLFWwindow* window, void* image, size_t width, size_t height);