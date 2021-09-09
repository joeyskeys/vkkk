#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "gui.h"

GLFWwindow* init_window(int width, int height) {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    return glfwCreateWindow(width, height, "Vkkk", nullptr, nullptr);
}