#include <functional>
#include <iostream>
#include <vector>
#include <optional>
#include <unordered_map>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

namespace vkkk
{

struct QueueFamilyIndex {
    std::optional<uint32_t> graphic_family;
    std::optional<uint32_t> present_family;

    inline bool is_valid() {
        return graphic_family.has_value() && present_family.has_value();
    }
};

class VkWrappedInstance {
public:
    VkWrappedInstance();
    VkWrappedInstance(uint32_t w, uint32_t h, const std::string& appname, const std::string& enginename);
    ~VkWrappedInstance();

    inline void setup_window(GLFWwindow* win) {
        window = win;
    }
    
    void create_surface();

    inline std::vector<VkPhysicalDevice> get_physical_devices() {
        return physical_devices;
    }

    bool validate_current_device(QueueFamilyIndex* idx);
    void create_logical_device();

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
    std::vector<VkPhysicalDevice> physical_devices;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device;

    // Multiple queues, currently store in a map with names
    using QueueMap = std::unordered_map<std::string, VkQueue>;
    QueueMap queue_map;
    VkQueue graphic_queue;
    VkQueue present_queue;

    // Surface
    VkSurfaceKHR surface;

    // Window, bound to glfw for now
    GLFWwindow* window;
};

}