#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "concepts/mesh.h"
#include "vk_ins/shader_mgr.h"
#include "vk_ins/types.h"

namespace fs = std::filesystem;

namespace vkkk
{

using ModernUpdateCBK = std::function<void(uint32_t swapchain_image_idx, VkCommandBuffer cmd, float delta_seconds)>;

struct ModernUBO {
    uint32_t size = 0;
    uint32_t vecsize = 1;
    std::vector<VkBuffer> buffers;
    std::vector<VkDeviceMemory> memories;
};

struct ModernMeshGPU {
    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VkDeviceMemory vertex_memory = VK_NULL_HANDLE;
    VkBuffer index_buffer = VK_NULL_HANDLE;
    VkDeviceMemory index_memory = VK_NULL_HANDLE;
    uint32_t index_count = 0;

    void upload(const Mesh& mesh, VkDevice device, VkPhysicalDevice physical_device,
        VkCommandPool command_pool, VkQueue queue);
    void draw(VkCommandBuffer cmd, VkPipelineLayout layout,
        VkDescriptorSet descriptor_set) const;
    void destroy(VkDevice device);
};

struct ModernPipeline {
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptor_layout = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptor_sets;
};

struct ModernGraphicsPipelineDesc {
    std::vector<ShaderModule> shader_modules;
    std::vector<VERT_COMP> vertex_components{VERTEX, NORMAL};
    VkCullModeFlags cull_mode = VK_CULL_MODE_NONE;
    bool depth_test = true;
    bool depth_write = true;
    bool blend_enable = false;
};

/// Vulkan 1.3 wrapper: dynamic rendering (no render passes / framebuffers),
/// per-swapchain depth targets, and a reduced resource surface area.
class VkWrappedInstanceModern {
public:
    VkWrappedInstanceModern() = default;
    VkWrappedInstanceModern(uint32_t width, uint32_t height, std::string app_name);
    ~VkWrappedInstanceModern();

    void init_glfw();
    void init();
    void create_swapchain_targets();
    void create_sync_objects();

    void set_update_callback(ModernUpdateCBK callback) { update_callback_ = std::move(callback); }
    void draw_frame();
    void mainloop();

    bool create_graphics_pipeline(const std::string& name, ModernGraphicsPipelineDesc desc);
    bool load_mesh(const std::string& name, const Mesh& mesh);
    ModernUBO& uniform_buffer(const std::string& name);
    void write_uniform(const std::string& name, uint32_t swapchain_index,
        const void* data, uint32_t size);

    void setup_key_callback(GLFWkeyfun callback) { glfwSetKeyCallback(window_, callback); }
    void setup_mouse_button_callback(GLFWmousebuttonfun callback) {
        glfwSetMouseButtonCallback(window_, callback);
    }
    void setup_cursor_pos_callback(GLFWcursorposfun callback) {
        glfwSetCursorPosCallback(window_, callback);
    }

    [[nodiscard]] GLFWwindow* window() const { return window_; }
    [[nodiscard]] VkInstance instance() const { return instance_; }
    [[nodiscard]] VkDevice device() const { return device_; }
    [[nodiscard]] VkPhysicalDevice physical_device() const { return physical_device_; }
    [[nodiscard]] VkQueue graphics_queue() const { return graphics_queue_; }
    [[nodiscard]] uint32_t graphics_queue_family() const { return graphics_queue_family_; }
    [[nodiscard]] VkExtent2D swapchain_extent() const { return swapchain_extent_; }
    [[nodiscard]] uint32_t swapchain_image_count() const {
        return static_cast<uint32_t>(swapchain_images_.size());
    }
    [[nodiscard]] VkFormat swapchain_format() const { return swapchain_format_; }
    [[nodiscard]] VkFormat depth_format() const { return depth_format_; }

    [[nodiscard]] ModernPipeline* find_pipeline(const std::string& name);
    [[nodiscard]] ModernMeshGPU* find_mesh(const std::string& name);

    void bind_graphics_pipeline(VkCommandBuffer cmd, VkPipeline pipeline) const;
    void cmd_set_viewport_scissor(VkCommandBuffer cmd) const;

private:
    static constexpr uint32_t kApiVersion = VK_API_VERSION_1_3;
    static constexpr int kMaxFramesInFlight = 2;

    static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
        void* user_data);

    static void framebuffer_resize_callback(GLFWwindow* window, int width, int height);

    std::vector<const char*> required_instance_extensions() const;
    std::vector<const char*> required_device_extensions() const;

    bool pick_physical_device();
    void create_instance();
    void create_device();
    void create_surface();
    void destroy_swapchain_targets();
    void recreate_swapchain();

    VkSurfaceFormatKHR choose_surface_format(const std::vector<VkSurfaceFormatKHR>& formats) const;
    VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR>& modes) const;
    VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR& capabilities) const;

    uint32_t find_memory_type(uint32_t filter, VkMemoryPropertyFlags properties) const;
    void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
        VkBuffer& buffer, VkDeviceMemory& memory) const;
    void copy_buffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) const;
    VkCommandBuffer begin_one_shot_commands() const;
    void end_one_shot_commands(VkCommandBuffer cmd) const;

    void transition_image_layout(VkCommandBuffer cmd, VkImage image, VkImageLayout old_layout,
        VkImageLayout new_layout, VkImageAspectFlags aspect_mask);
    void create_depth_resources();
    void allocate_command_buffers();

    void record_and_submit(uint32_t image_index);

private:
    uint32_t width_ = 800;
    uint32_t height_ = 600;
    std::string app_name_ = "vkkk modern";

    GLFWwindow* window_ = nullptr;
    bool framebuffer_resized_ = false;

    VkInstance instance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;

    VkQueue graphics_queue_ = VK_NULL_HANDLE;
    VkQueue present_queue_ = VK_NULL_HANDLE;
    uint32_t graphics_queue_family_ = 0;
    uint32_t present_queue_family_ = 0;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchain_format_ = VK_FORMAT_B8G8R8A8_SRGB;
    VkExtent2D swapchain_extent_{};
    std::vector<VkImage> swapchain_images_;
    std::vector<VkImageView> swapchain_image_views_;

    VkFormat depth_format_ = VK_FORMAT_D32_SFLOAT;
    std::vector<VkImage> depth_images_;
    std::vector<VkDeviceMemory> depth_memories_;
    std::vector<VkImageView> depth_image_views_;

    VkCommandPool command_pool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> command_buffers_;

    std::vector<VkSemaphore> image_available_semaphores_;
    std::vector<VkSemaphore> render_finished_semaphores_;
    std::vector<VkFence> in_flight_fences_;
    std::vector<VkFence> images_in_flight_;
    uint32_t current_frame_ = 0;

    std::chrono::steady_clock::time_point last_frame_time_{};

    ModernUpdateCBK update_callback_;
    std::unordered_map<std::string, ModernPipeline> pipelines_;
    std::unordered_map<std::string, ModernMeshGPU> meshes_;
    std::unordered_map<std::string, ModernUBO> uniform_buffers_;
};

} // namespace vkkk
