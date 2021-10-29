#include <functional>

#include <vulkan/vulkan.h>

void create_vk_instance(
    const std::string& app_name,
    const std::string& engine_name,
    std::function<vector<char*>(VkDebugUtilsMessagerCreateInfoEXT&)> validation_layer_functor);

void init_vulkan();