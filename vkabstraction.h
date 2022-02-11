#include <array>
#include <functional>
#include <iostream>
#include <span>
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

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
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
    void create_swapchain();
    void create_imageviews();
    VkShaderModule create_shader_module(std::vector<char>&);
    void create_graphics_pipeline();

private:
    // Private methods
    std::vector<const char*> get_default_instance_extensions();
    inline std::array<const char*, 1> get_default_device_extensions() {
        return { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL default_debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* cb_data,
        void* user_data) {
        std::cerr << "validation layer: " << cb_data->pMessage << std::endl;
        return VK_FALSE;
    }

    bool check_device_extension_support(const std::span<const char*> extensions) const;

    VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats) const;
    VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available_present_modes) const;
    VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities) const;


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

    // Surface
    VkSurfaceKHR surface;

    // Multiple queues, currently store in a map with names
    using QueueIndexVec = std::vector<uint32_t>;
    QueueIndexVec queue_idx_vec;
    VkQueue graphic_queue;
    VkQueue present_queue;
    bool    queue_created = false;

    // SwapChain related
    VkSwapchainKHR              swapchain;
    std::vector<VkImage>        swapchain_images;
    SwapChainSupportDetails     swapchain_details;
    VkSurfaceFormatKHR          swapchain_surface_format;
    VkExtent2D                  swapchain_extent;
    bool                        swapchain_created = false;

    std::vector<VkImageView>    swapchain_imageviews;
    bool                        imageviews_created = false;

    // Window, bound to glfw for now
    GLFWwindow*     window;
};

}