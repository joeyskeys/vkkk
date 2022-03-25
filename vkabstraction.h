#include <array>
#include <chrono>
#include <functional>
#include <iostream>
#include <span>
#include <vector>
#include <optional>
#include <unordered_map>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;

    static VkVertexInputBindingDescription get_binding_description() {
        VkVertexInputBindingDescription des{};
        des.binding = 0;
        des.stride = sizeof(Vertex);
        des.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return des;
    }

    static auto get_attr_descriptions() {
        std::array<VkVertexInputAttributeDescription, 2> des{};

        des[0].binding = 0;
        des[0].location = 0;
        des[0].format = VK_FORMAT_R32G32_SFLOAT;
        des[0].offset = offsetof(Vertex, pos);

        des[1].binding = 0;
        des[1].location = 1;
        des[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        des[1].offset = offsetof(Vertex, color);

        return des;
    }
};

struct MVPBuffer {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
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
    void cleanup_swapchain();
    void recreate_swapchain();
    void create_imageviews();
    void create_renderpass();
    VkShaderModule create_shader_module(std::vector<char>&);
    void create_descriptor_set_layout();
    void create_graphics_pipeline();
    void create_framebuffers();
    void create_command_pool();
    void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
        VkMemoryPropertyFlags props, VkBuffer& buf, VkDeviceMemory& buf_memo);
    void copy_buffer(VkBuffer src_buf, VkBuffer dst_buf, VkDeviceSize size);
    void create_vertex_buffer(const Vertex* source_data, size_t vcnt);
    void create_index_buffer(const uint32_t* index_data, size_t idx_cnt);
    void create_uniform_buffer();
    void update_uniform_buffer(uint32_t idx);
    void create_descriptor_pool();
    void create_descriptor_set();
    void create_commandbuffers();
    void create_sync_objects();
    void draw_frame();
    void mainloop();

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

    static void default_resize_callback(GLFWwindow* window, int w, int h) {
        auto app = reinterpret_cast<VkWrappedInstance*>(glfwGetWindowUserPointer(window));
        app->framebuffer_resized = true;
    }

    bool check_device_extension_support(const std::span<const char*> extensions) const;

    VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats) const;
    VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available_present_modes) const;
    VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities) const;
    uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags props) const;

private:
    uint32_t width = 800;
    uint32_t height = 600;
    std::string app_name = "vkkk";
    std::string engine_name = "vulkan";
    uint32_t app_version = VK_MAKE_VERSION(1, 0, 0);
    uint32_t api_version = VK_API_VERSION_1_1;
    bool enable_validation_layers = true;

    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;

    QueueFamilyIndex queue_family_idx;

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
    VkSwapchainKHR                  swapchain;
    std::vector<VkImage>            swapchain_images;
    SwapChainSupportDetails         swapchain_details;
    VkSurfaceFormatKHR              swapchain_surface_format;
    VkExtent2D                      swapchain_extent;
    bool                            swapchain_created = false;

    std::vector<VkImageView>        swapchain_imageviews;
    bool                            imageviews_created = false;

    // Pipeline related
    VkPipelineLayout                pipeline_layout;
    VkPipeline                      pipeline;
    bool                            pipeline_created = false;
    VkRenderPass                    render_pass;
    bool                            render_pass_created = false;
    VkDescriptorSetLayout           descriptor_layout;
    bool                            descriptor_layout_created = false;
    VkDescriptorPool                descriptor_pool;
    bool                            descriptor_pool_created = true;
    std::vector<VkDescriptorSet>    descriptor_sets;

    // Buffers
    std::vector<VkFramebuffer>      swapchain_framebuffers;
    bool                            framebuffer_created = false;
    bool                            framebuffer_resized = false;

    VkCommandPool                   command_pool;
    bool                            commandpool_created = false;
    std::vector<VkCommandBuffer>    commandbuffers;
    bool                            commandbuffer_created = false;

    // Semaphore and fences
    std::vector<VkSemaphore>        image_available_semaphores;
    std::vector<VkSemaphore>        render_finished_semaphores;
    std::vector<VkFence>            in_flight_fences;
    std::vector<VkFence>            images_in_flight;
    bool                            syncobj_created = false;

    // Frame related
    size_t                          current_frame = 0;

    // Vertex Buffer
    VkBuffer                        vert_buffer;
    VkDeviceMemory                  vert_buffer_memo;
    bool                            vertbuffer_created = false;
    VkBuffer                        index_buffer;
    VkDeviceMemory                  index_buffer_memo;
    bool                            indexbuffer_created = false;
    std::vector<VkBuffer>           uniform_buffers;
    std::vector<VkDeviceMemory>     uniform_buffer_memos;
    bool                            uniform_buffer_created = false;

    // Window, bound to glfw for now
    GLFWwindow*     window;
};

}