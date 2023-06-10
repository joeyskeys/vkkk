#include <iostream>
#include <array>

#include <GLFW/glfw3.h>

#include "vk_ins/vkabstraction.h"

constexpr static uint32_t WIDTH = 800;
constexpr static uint32_t HEIGHT = 600;

const std::array<const char*, 1> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

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
    auto swapchain_img_cnt = ins.create_swapchain();
    ins.create_imageviews();
    ins.create_renderpass();
    ins.create_descriptor_set_layout();
    ins.create_graphics_pipeline();
    ins.create_command_pool();
    ins.create_descriptor_pool();
    ins.create_descriptor_set();
    ins.create_uniform_buffer();
    ins.create_commandbuffers();
    ins.create_sync_objects();
    ins.mainloop();
    return 0;
}