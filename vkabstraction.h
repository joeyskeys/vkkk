#include <functional>
#include <iostream>
#include <vector>
#include <optional>
#include <unordered_map>

#include <vulkan/vulkan.h>

namespace vkkk
{

using QueueFamilyIndex = std::optional<uint32_t>;

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

bool validate_device(const VkPhysicalDevice& device, const VkQueueFlagBits& flags, QueueFamilyIndex* idx);

void create_logical_device(const VkPhysicalDevice& physical_device, const VkQueueFlagBits& flags, VkDevice& device);

class VkWrappedInstance {
public:
    VkWrappedInstance();
    VkWrappedInstance(uint32_t w, uint32_t h, const std::string& appname, const std::string& enginename);
    ~VkWrappedInstance();

    bool init();

private:
    // Private methods
    std::vector<const char*> get_default_extensions();
    static VKAPI_ATTR VkBool32 VKAPI_CALL default_debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* cb_data,
        void* user_data) {
        std::cerr << "validation layer: " << cb_data->pMessage << std::endl;
        return VK_FALSE;
    }

private:
    uint32_t width = 800;
    uint32_t height = 600;
    std::string app_name = "vkkk";
    std::string engine_name = "vulkan";
    uint32_t app_version = VK_MAKE_VERSION(1, 0, 0);
    uint32_t api_version = VK_API_VERSION_1_2;
    bool enable_validation_layers = true;

    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;

    // Currently only use one physical card and one logical device
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device;

    // Multiple queues, currently store in a map with names
    using QueueMap = std::unordered_map<std::string, VkQueue>;
    QueueMap queue_map;
};

}