#include <iostream>
#include <array>

#include <GLFW/glfw3.h>

#include "gui.h"
#include "vkabstraction.h"

const static unsigned int WIDTH = 800;
const static unsigned int HEIGHT = 600;

const std::array<const char*, 1> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

bool check_validation_layer_support() {
    uint32_t layer_cnt;
    vkEnumerateInstanceLayerProperties(&layer_cnt, nullptr);

    std::vector<VkLayerProperties> availableLayers(layer_cnt);
    vkEnumerateInstanceLayerProperties(&layer_cnt, availableLayers.data());

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

int main() {
    auto win = init_window(800, 600);

    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;
    const char** glfw_extensions;
    uint32_t glfw_extension_cnt;
    glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_cnt);
    auto extensions = std::vector<const char*>(glfw_extensions, glfw_extensions + glfw_extension_cnt);
    vkkk::create_instance(instance, "vkkk", "vkbackend", extensions, vkkk::default_validation_layer_func, &debug_messenger);
    return 0;
}