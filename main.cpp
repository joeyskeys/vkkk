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
    vkkk::VkWrappedInstance ins;
    ins.create_surface();
    ins.create_logical_device();
    ins.create_swapchain();
    ins.create_imageviews();
    ins.create_renderpass();
    ins.create_graphics_pipeline();
    ins.create_framebuffers();
    ins.create_command_pool();
    
    const std::array<vkkk::Vertex, 4> verts{
        glm::vec2{-0.5f, -0.5f}, glm::vec3{1.f, 0.f, 0.f},
        glm::vec2{0.5f, -0.5f}, glm::vec3{0.f, 1.f, 0.f},
        glm::vec2{0.5f, 0.5f}, glm::vec3{0.f, 0.f, 1.f},
        glm::vec2{-0.5f, 0.5f}, glm::vec3{1.f, 1.f, 1.f}
    };
    ins.create_vertex_buffer(verts.data(), verts.size());

    const std::array<uint32_t, 6> indices{
        0, 1, 2, 2, 3, 0
    };
    ins.create_index_buffer(indices.data(), indices.size());

    ins.create_commandbuffers();
    ins.create_sync_objects();

    ins.mainloop();

    return 0;
}