#pragma once

#include <cstdint>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

#include "vk_ins/shader_module_pack.hpp"

namespace vkkk
{

namespace
{

const std::vector<const char*> default_validation_layers = {
    "VK_LAYER_KHRONOS_validation"
};

constexpr char* default_app_name = "vkkk";
constexpr char* default_engine_name = "vulkan";
constexpr uint32_t default_app_version = VK_MAKE_VERSION(1, 0, 0);
constexpr uint32_t default_api_version = vk::ApiVersion14;
constexpr uint32_t max_frames_in_flight = 2;

}

struct Pipeline {
    vk::raii::Pipeline vk_pipeline;
    vk::raii::PipelineLayout vk_pipeline_layout;
};

struct PipelineOption {
    vk::PipelineVertexInputStateCreateInfo vert_info;
    vk::PipelineInputAssemblyStateCreateInfo assembly_info;
    vk::PipelineViewportStateCreateInfo viewport_info;
    vk::PipelineRasterizationStateCreateInfo raster_info;
    vk::PipelineMultisampleStateCreateInfo multisample_info;
    vk::PipelineDepthStencilStateCreateInfo depth_info;
    vk::PipelineColorBlendAttachmentState blend_attachment_info;
    vk::PipelineColorBlendStateCreateInfo blend_info;
    vk::PipelineDynamicStateCreateInfo dynamic_info;
};

// a class manages vulkan instance, physical device, logical device, surface
// these parts are not frequently changed or used.
class WrappedContext {
public:
    WrappedContext(
        const char* app_name = default_app_name,
        uint32_t app_version = default_app_version,
        const char* engine_name = default_engine_name,
        uint32_t api_version = default_api_version,
        bool enable_validation_layers = true,
        const std::vector<const char*>& extra_validation_layers = {},
        const std::vector<const char*>& extra_extensions = {},
        bool enable_debug_messenger = true
      ) noexcept;

    void init(GLFWwindow* window);

    bool create_pipeline(const std::string& name, const ShaderModulePack& shader_module_pack, const PipelineOption& option);
    void record_cmds(const std::function<void(uint32_t)>& emit_func);
    void draw_frame();
    void recreate_swapchain();

private:
    void setup_debug_messenger();
    void transit_image_layout(
        uint32_t image_index,
        vk::ImageLayout old_layout,
        vk::ImageLayout new_layout,
        vk::AccessFlags2 src_access_mask,
        vk::AccessFlags2 dst_access_mask,
        vk::PipelineStageFlags2 src_stage_mask,
        vk::PipelineStageFlags2 dst_stage_mask,
    );
    void create_swapchain();
    void create_imageviews();

private:
    std::vector<const char*> required_extensions = {
        vk::KHRSwapchainExtensionName
    };

    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;
    vk::raii::DebugUtilMessenger debug_messenger = nullptr;

    vk::raii::SurfaceKHR surface = nullptr;
    vk::raii::PhysicalDevice physical_device = nullptr;
    vk::raii::Device device = nullptr;
    vk::raii::Queue queue = nullptr;
    uint32_t queue_idx = ~0;

    vk::raii::SwapchainKHR swapchain = nullptr;
    std::vector<vk::Image> swapchain_images;
    std::vector<vk::raii::ImageView> swapchain_image_views;
    vk::Extent2D swapchain_extent;
    vk::SurfaceFormatKHR swapchain_surface_format;

    vk::raii::CommandPool command_pool = nullptr;

    std::vector<vk::raii::Semaphore> present_complete_semaphores;
    std::vector<vk::raii::Semaphore> render_finished_semaphores;
    std::vector<vk::raii::Fence> in_flight_fences;

    uint32_t current_frame = 0;

public:
    std::unordered_map<std::string, Pipeline> pipelines;
    std::vector<vk::raii::CommandBuffer> command_buffers;
    bool frame_buffer_resized = false;
};

}