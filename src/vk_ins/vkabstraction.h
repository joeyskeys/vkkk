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

#include "concepts/mesh.h"
#include "asset_mgr/mesh_mgr.h"
#include "vk_ins/cmd_buf.h"
#include "vk_ins/render_target.h"
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

using UpdateCBK = std::function<void(uint32_t, float)>;

/************************************************************
 * A design problem occurred now:
 * Seperate vk objects into different classes will have us to
 * explicitly call free_gpu_resources function to do the job
 * instead of relying on RAII(Since the instance could be freed
 * before other resources).
 * Another problem is it will hurt when we're creating bindings
 * for these functions when we're passing vk objects around,
 * which will require us to add bindings for basically all the
 * vk natives class we used in the function interface and that
 * will be a lot of work.
 * Now considering to put all the vulkan resources into the
 * wrapped instance and keep a pointer or index in the abstract
 * classes.
 ************************************************************/

class VkWrappedInstance {
public:
    VkWrappedInstance();
    VkWrappedInstance(uint32_t w, uint32_t h, const std::string& appname, const std::string& enginename);
    ~VkWrappedInstance();

    static inline void print_validation_layer_supports() {
        uint32_t layer_cnt;
        vkEnumerateInstanceLayerProperties(&layer_cnt, nullptr);
        std::vector<VkLayerProperties> layers(layer_cnt);
        vkEnumerateInstanceLayerProperties(&layer_cnt, layers.data());
        for (const auto& layer : layers)
            std::cout << layer.layerName << std::endl;
    }

    void list_physical_devices() const;
    void choose_device(uint32_t i);

    void init_glfw();
    void init();

    inline void setup_window(GLFWwindow* win) {
        window = win;
    }

    VkCommandBuffer begin_single_time_commands();
    void end_single_time_commands(VkCommandBuffer cmd_buf);

    void create_vk_image(const uint32_t w, const uint32_t h, const uint32_t layers,
        const VkSampleCountFlagBits n, const VkFormat format, VkImageTiling tiling,
        VkImageUsageFlags usage, VkImageCreateFlags flags, VkMemoryPropertyFlags properties,
        VkImage& image, VkDeviceMemory& image_memo);
    void transition_image_layout(VkImage image, VkFormat format,
        VkImageLayout old_layout, VkImageLayout new_layout, VkImageSubresourceRange sub_range);
    void copy_buffer_to_image(VkBuffer buf, VkImage image, const std::vector<VkBufferImageCopy>& regions);
    
    void create_surface();
    VkSampleCountFlagBits get_max_usable_sample_cnt() const;

    inline std::vector<VkPhysicalDevice> get_physical_devices() {
        return physical_devices;
    }

    inline VkPhysicalDeviceMemoryProperties get_memory_props() {
        return mem_props;
    }

    inline VkDevice get_device() {
        return device;
    }

    inline VkQueue get_graphic_queue() {
        return graphic_queue;
    }

    inline auto get_swapchain() {
        return swapchain;
    }

    inline auto get_swapchain_cnt() {
        return swapchain_cnt;
    }

    inline auto get_swapchain_format() {
        return swapchain_surface_format;
    }

    inline auto get_renderpass() {
        return render_pass;
    }

    inline auto& get_framebuffers() {
        return swapchain_framebuffers;
    }

    inline auto& get_command_pool() {
        return command_pool;
    }

    inline auto& get_swapchain_extent() {
        return swapchain_extent;
    }

    inline void set_update_cbk(UpdateCBK cbk) {
        update_cbk = cbk;
    }

    bool validate_current_device(QueueFamilyIndex* idx);
    void create_logical_device();
    uint32_t create_swapchain();
    void cleanup_swapchain();
    void recreate_swapchain();
    VkImageView create_imageview(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags);
    void create_imageviews();
    void create_renderpass();
    void create_framebuffers();
    void create_command_pool();
    
    void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
        VkMemoryPropertyFlags props, VkBuffer &buf, VkDeviceMemory& buf_memo);
    void copy_buffer(VkBuffer src_buf, VkBuffer dst_buf, VkDeviceSize size);
    void create_vertex_buffer(const float *, VkBuffer&, VkDeviceMemory&, size_t, size_t);
    void create_index_buffer(const uint32_t*, VkBuffer&, VkDeviceMemory&, size_t);
    void create_color_resource();
    void create_depth_resource();

    inline void create_resources(VkSampleCountFlagBits ns) {
        create_surface();
        create_logical_device();
        create_swapchain();
        create_imageviews();
        nsample = ns;
        create_renderpass();
        create_command_pool();
        create_color_resource();
        create_depth_resource();
        create_framebuffers();
    }

    void create_descriptors(const ShaderModules& modules);
    void alloc_commandbuffers(std::vector<VkCommandBuffer>& bufs);
    void record_cmds(std::vector<VkCommandBuffer>& cmd_bufs, std::vector<VkFramebuffer>& fbs,
        const std::function<void(uint32_t)>& emit_func);
    void create_sync_objects();
    void draw_frame(const CommandBuffers&);
    void mainloop(const CommandBuffers&);

    using KeyCBK = void(*)(GLFWwindow*, int, int, int, int);
    void setup_key_cbk(KeyCBK);
    using MouseBtnCBK = void(*)(GLFWwindow*, int, int, int);
    void setup_mouse_btn_cbk(MouseBtnCBK);
    using MousePosCBK = void(*)(GLFWwindow*, double, double);
    void setup_mouse_pos_cbk(MousePosCBK);

    // Utils
    std::unique_ptr<char[]> get_image_buffer(const RenderTarget& rt) const;

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

    inline void init_time() {
        time = std::chrono::high_resolution_clock::now();
    }

private:
    uint32_t width = 800;
    uint32_t height = 600;
    std::string app_name = "vkkk";
    std::string engine_name = "vulkan";
    uint32_t app_version = VK_MAKE_VERSION(1, 0, 0);
    uint32_t api_version = VK_API_VERSION_1_1;
    uint32_t swapchain_cnt = 0;
    bool enable_validation_layers = true;

    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;

    QueueFamilyIndex queue_family_idx;

    // Uniform update callback
    UpdateCBK           update_cbk;

    // Currently only use one physical card and one logical device
    std::vector<VkPhysicalDevice> physical_devices;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties physical_device_props;
    VkPhysicalDeviceMemoryProperties mem_props;
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
    VkRenderPass                    render_pass;
    bool                            render_pass_created = false;

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

    // Cannot delete the two field here for now or it will cause a weird
    // render bug, I believe it's relavent to data padding since these two
    // field are not used any where and I can use any field that have equal
    // length here
    // Vertex Buffer
    //VkBuffer                        index_buffer;
    //VkDeviceMemory                  index_buffer_memo;
    double                          padding[2];

    // Color resource
    VkImage                         color_img;
    VkDeviceMemory                  color_img_memo;
    VkImageView                     color_img_view;
    bool                            color_created = false;

    // Depth Buffer
    VkImage                         depth_img;
    VkDeviceMemory                  depth_img_memo;
    VkImageView                     depth_img_view;
    bool                            depth_created = false;

    // Window, bound to glfw for now
    GLFWwindow*                     window;
    std::chrono::time_point<std::chrono::system_clock>  time;

public:
    VkSampleCountFlagBits           nsample = VK_SAMPLE_COUNT_1_BIT;
};

}