#include <iostream>

#include <GLFW/glfw3.h>

#include "vkabstraction.h"

const static unsigned int WIDTH = 800;
const static unsigned int HEIGHT = 600;

void init_window() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(WIDHT, HEIGHT, "vkkk", nullptr, nullptr);
}

int main() {
    init_window();
    init_vulkan();
    return 0;
}