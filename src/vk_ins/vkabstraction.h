#include <array>
#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <span>
#include <vector>
#include <optional>
#include <unordered_map>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "asset_mgr/mesh.h"
#include "vk_ins/shader_mgr.h"

namespace fs = std::filesystem;

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

struct Pixel {
    char r;
    char g;
    char b;
    char a;
};

struct VertexTmp {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 uv;

    static VkVertexInputBindingDescription get_binding_description() {
        VkVertexInputBindingDescription des{};
        des.binding = 0;
        des.stride = sizeof(VertexTmp);
        des.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return des;
    }

    static auto get_attr_descriptions() {
        std::array<VkVertexInputAttributeDescription, 3> des{};

        des[0].binding = 0;
        des[0].location = 0;
        des[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        des[0].offset = offsetof(VertexTmp, pos);

        des[1].binding = 0;
        des[1].location = 1;
        des[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        des[1].offset = offsetof(VertexTmp, color);

        des[2].binding = 0;
        des[2].location = 2;
        des[2].format = VK_FORMAT_R32G32_SFLOAT;
        des[2].offset = offsetof(VertexTmp, uv);

        return des;
    }
};

struct MVPBuffer {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

using UnifromUpdateCBK = std::function<void(MVPBuffer*)>;

class VkWrappedInstance {
public:
    VkWrappedInstance();
    VkWrappedInstance(uint32_t w, uint32_t h, const std::string& appname, const std::string& enginename);
    ~VkWrappedInstance();

    inline void setup_window(GLFWwindow* win) {
        window = win;
    }

    VkCommandBuffer begin_single_time_commands();
    void end_single_time_commands(VkCommandBuffer cmd_buf);

    void create_vk_image(const uint32_t w, const uint32_t h, const VkFormat format,
        VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
        VkImage& image, VkDeviceMemory& image_memo);
    void transition_image_layout(VkImage image, VkFormat format,
        VkImageLayout old_layout, VkImageLayout new_layout);
    void copy_buffer_to_image(VkBuffer buf, VkImage image, uint32_t w,
        uint32_t h);
    void create_texture_imageviews();
    void create_texture_sampler();
    bool load_texture(const fs::path& path);
    
    void create_surface();

    inline std::vector<VkPhysicalDevice> get_physical_devices() {
        return physical_devices;
    }

    inline VkDevice get_device() {
        return device;
    }

    inline void set_uniform_cbk(UnifromUpdateCBK cbk) {
        uniform_cbk = cbk;
    }

    bool validate_current_device(QueueFamilyIndex* idx);
    void create_logical_device();
    void create_swapchain();
    void cleanup_swapchain();
    void recreate_swapchain();
    VkImageView create_imageview(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags);
    void create_imageviews();
    void create_renderpass();
    VkShaderModule create_shader_module(std::vector<char>&);
    void create_descriptor_set_layout();

    void create_graphics_pipeline();
    void create_graphics_pipeline(
        const ShaderModules&,
        const uint32_t,
        const VkPrimitiveTopology,
        const VkPolygonMode);

    void create_framebuffers();
    void create_command_pool();
    void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
        VkMemoryPropertyFlags props, VkBuffer &buf, VkDeviceMemory& buf_memo);
    void copy_buffer(VkBuffer src_buf, VkBuffer dst_buf, VkDeviceSize size);
    void create_vertex_buffer(const VertexTmp *source_data, size_t vcnt);
    void create_vertex_buffer(const float *source_data, size_t comp_size, size_t vcnt);
    void create_index_buffer(const uint32_t* index_data, size_t idx_cnt);
    void create_uniform_buffer();
    void update_uniform_buffer(uint32_t idx);
    void create_depth_resource();
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

    VkFormat find_supported_format(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    inline VkFormat find_depth_format() {
        return find_supported_format(
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }

    inline bool has_stencil_comp(VkFormat format) {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

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

    // Uniform update callback
    UnifromUpdateCBK uniform_cbk;

    // Currently only use one physical card and one logical device
    std::vector<VkPhysicalDevice> physical_devices;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device;

    // Textures
    std::vector<std::vector<Pixel>> texture_bufs;
    std::vector<VkImage>            vk_images;
    std::vector<VkDeviceMemory>     vk_image_memos;
    std::vector<VkImageView>        texture_views;
    VkImage                         tex_img;
    VkImageLayout                   tex_layout;
    VkDeviceMemory                  tex_img_memo;
    VkImageView                     tex_view;
    VkSampler                       texture_sampler;
    bool                            sampler_created;

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

    // Depth Buffer
    VkImage                         depth_img;
    VkDeviceMemory                  depth_img_memo;
    VkImageView                     depth_img_view;
    bool                            depth_created;

    // Window, bound to glfw for now
    GLFWwindow*                     window;
};

}