#include <iostream>
#include <vector>

#include <GLFW/glfw3.h>

#include "vkabstraction.h"

const static unsigned int WIDTH = 800;
const static unsigned int HEIGHT = 600;

const std::vector<const char*, 1> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

bool check_validation_layer_support() {
    uint32_t layer_cnt;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layer_cnt);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

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