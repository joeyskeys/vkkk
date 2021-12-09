#include <functional>
#include <iostream>
#include <vector>

#include <vulkan/vulkan.h>

namespace vkkk
{

std::vector<const char*> default_validation_layer_func();

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
    void* p_user_data) {
    std::cerr << "validation layer: " << p_callback_data->pMessage << std::endl;
    return VK_FALSE;
}

void create_instance(
    VkInstance& instance,
    const std::string& app_name,
    const std::string& engine_name,
    std::vector<const char*> extensions,
    std::function<std::vector<const char*>()> validation_layer_functor,
    VkDebugUtilsMessengerEXT* debug_messenger,
    PFN_vkDebugUtilsMessengerCallbackEXT debug_cbk=debug_callback);

std::vector<VkPhysicalDevice> get_physical_devices(const VkInstance& instance);

bool validate_device(const VkPhysicalDevice& device, const VkQueueFlagBits& flags);

void create_logical_device();

}