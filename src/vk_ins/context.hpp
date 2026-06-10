#pragma once

#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_raii.hpp>

#include "concepts/mesh.h"
#include "vk_ins/shader_module_pack.hpp"
#include "vk_ins/types.h"

namespace fs = std::filesystem;

namespace vkkk
{

namespace
{

const std::vector<const char*> default_validation_layers = {
    "VK_LAYER_KHRONOS_validation"
};

constexpr const char* default_app_name = "vkkk";
constexpr const char* default_engine_name = "vulkan";
constexpr uint32_t default_app_version = VK_MAKE_VERSION(1, 0, 0);
constexpr uint32_t default_api_version = vk::ApiVersion14;
constexpr uint32_t max_frames_in_flight = 2;

}

class Camera;

struct Pipeline {
    vk::raii::Pipeline vk_pipeline{nullptr};
    vk::raii::PipelineLayout vk_pipeline_layout{nullptr};
    vk::raii::DescriptorSetLayout descriptor_set_layout{nullptr};
    vk::raii::DescriptorPool descriptor_pool{nullptr};
    std::vector<vk::raii::DescriptorSet> descriptor_sets;
};

struct PipelineOption {
    PipelineOption();

    vk::PipelineVertexInputStateCreateInfo vert_info;
    vk::PipelineInputAssemblyStateCreateInfo assembly_info;
    vk::Viewport viewport;
    vk::PipelineViewportStateCreateInfo viewport_info;
    vk::Rect2D scissor;
    vk::PipelineRasterizationStateCreateInfo raster_info;
    vk::PipelineMultisampleStateCreateInfo multisample_info;
    vk::PipelineDepthStencilStateCreateInfo depth_info;
    vk::PipelineColorBlendAttachmentState blend_attachment_info;
    vk::PipelineColorBlendStateCreateInfo blend_info;
    std::vector<vk::DynamicState> dynamic_states;
    vk::PipelineDynamicStateCreateInfo dynamic_info;

    inline void setup_input_assembly(vk::PrimitiveTopology topo, bool restart) {
        assembly_info.topology = topo;
        assembly_info.primitiveRestartEnable = restart ? vk::True : vk::False;
    }

    inline void setup_viewport(float x, float y, float w, float h, float min_depth, float max_depth) {
        viewport.x = x;
        viewport.y = y;
        viewport.width = w;
        viewport.height = h;
        viewport.minDepth = min_depth;
        viewport.maxDepth = max_depth;
    }

    inline void setup_scissor(int32_t off_x, int32_t off_y, uint32_t ext_x, uint32_t ext_y) {
        scissor.offset = vk::Offset2D{off_x, off_y};
        scissor.extent = vk::Extent2D{ext_x, ext_y};
    }

    inline void setup_rasterizer(bool depth_clamp, bool discard, vk::PolygonMode mode, float line_width,
        vk::CullModeFlags cull_mode, vk::FrontFace front, bool depth_bias)
    {
        raster_info.depthClampEnable = depth_clamp ? vk::True : vk::False;
        raster_info.rasterizerDiscardEnable = discard ? vk::True : vk::False;
        raster_info.polygonMode = mode;
        raster_info.lineWidth = line_width;
        raster_info.cullMode = cull_mode;
        raster_info.frontFace = front;
        raster_info.depthBiasEnable = depth_bias ? vk::True : vk::False;
    }

    inline void setup_multisampling(bool enable, vk::SampleCountFlagBits sample_count) {
        multisample_info.sampleShadingEnable = enable ? vk::True : vk::False;
        multisample_info.rasterizationSamples = sample_count;
    }

    inline void setup_depth_stencil(bool test_enable, bool write_enable, vk::CompareOp compare_op,
        bool bounds_enable, bool stencil_enable)
    {
        depth_info.depthTestEnable = test_enable ? vk::True : vk::False;
        depth_info.depthWriteEnable = write_enable ? vk::True : vk::False;
        depth_info.depthCompareOp = compare_op;
        depth_info.depthBoundsTestEnable = bounds_enable ? vk::True : vk::False;
        depth_info.stencilTestEnable = stencil_enable ? vk::True : vk::False;
    }
};

struct UBO {
    size_t                                  size;
    size_t                                  vecsize;
    uint32_t                                binding;
    std::shared_ptr<char[]>                 cpu_buf;
    std::vector<vk::raii::Buffer>           gpu_bufs;
    std::vector<vk::raii::DeviceMemory>     memos;
    std::vector<vk::DescriptorBufferInfo>   descriptors;
};

struct Texture {
    uint32_t                                binding;
    size_t                                  vecsize;
    vk::raii::Image                         image{nullptr};
    vk::raii::DeviceMemory                  memo{nullptr};
    vk::raii::ImageView                     view{nullptr};
    vk::ImageLayout                         layout;
    vk::DescriptorImageInfo                 descriptor;
    vk::raii::Sampler                       sampler{nullptr};
};

struct MeshGPU {
    vk::raii::Buffer                        vbuf{nullptr};
    vk::raii::DeviceMemory                  vbuf_memo{nullptr};
    vk::raii::Buffer                        ibuf{nullptr};
    vk::raii::DeviceMemory                  ibuf_memo{nullptr};
    uint32_t                                icnt = 0;
    
    void sync(const Mesh& mesh, class Context* ctx);
    void emit_draw_cmd(vk::CommandBuffer cmd_buf, vk::PipelineLayout ppl_layout,
        const vk::DescriptorSet* desc_set = nullptr) const;
};

struct CameraGPU {
    uint32_t                                binding;
    vk::raii::Buffer                        buf{nullptr};
    vk::raii::DeviceMemory                  memo{nullptr};
    vk::DescriptorBufferInfo                descriptor;

    void sync(Camera& cam, Context* ctx) const;
};

// a class manages vulkan instance, physical device, logical device, surface
// these parts are not frequently changed or used.
class Context {
public:
    Context(
        const char* app_name = default_app_name,
        uint32_t app_version = default_app_version,
        const char* engine_name = default_engine_name,
        uint32_t api_version = default_api_version,
        bool enable_validation_layers = true,
        const std::vector<const char*>& extra_validation_layers = {},
        const std::vector<const char*>& extra_extensions = {},
        bool enable_debug_messenger = true);

    static std::vector<const char*> get_glfw_instance_extensions();
    static GLFWwindow* create_window(int width, int height, const char* title, bool resizable = false);

    void init(GLFWwindow* window);

    bool create_pipeline(const std::string& name,
        const ShaderModulePack& shader_module_pack,
        const PipelineOption& option,
        const std::vector<VERT_COMP>& comps,
        bool interleaved = true);
    void draw_frame();
    void wait_idle() const { device.waitIdle(); }
    void recreate_swapchain();
    void record_cmds(uint32_t image_index,
        const std::function<void(vk::raii::CommandBuffer&, uint32_t)>& emit_func);

    bool add_ubo(const std::string& name, uint32_t binding,
        uint32_t size, uint32_t vecsize = 1);
    bool add_texture(const std::string& name, uint32_t binding,
        const fs::path& path);
    bool add_cubemap(const std::string& name, uint32_t binding,
        const fs::path& path);
    bool load_mesh(const std::string& name, const Mesh& mesh);

    void create_vertex_buffer(const float* src, vk::raii::Buffer& buf, vk::raii::DeviceMemory& memo,
        size_t comp_size, size_t vcnt) const
    {
        create_input_attr_buffer<vk::BufferUsageFlagBits::eVertexBuffer>(
            src, buf, memo, comp_size * sizeof(float), vcnt);
    }

    void create_index_buffer(const uint32_t* src, vk::raii::Buffer& buf, vk::raii::DeviceMemory& memo,
        size_t idx_cnt) const
    {
        create_input_attr_buffer<vk::BufferUsageFlagBits::eIndexBuffer>(
            reinterpret_cast<const float*>(src), buf, memo, sizeof(uint32_t), idx_cnt);
    }

    void sync_uniform(const vk::raii::DeviceMemory& memo, const void* data, uint32_t size) const;
    UBO& require_ubo(const std::string& full_name);
    GLFWwindow* get_window() const { return window_; }

    using UpdateCallback = std::function<void(uint32_t image_index, float dt)>;
    void set_update_cbk(UpdateCallback cbk) { update_cbk_ = std::move(cbk); }

    vk::SampleCountFlagBits nsample = vk::SampleCountFlagBits::e1;

private:
    void setup_debug_messenger();
    void transit_presentation_image_layout(
        vk::raii::CommandBuffer& cmd_buf,
        vk::Image img,
        vk::ImageLayout old_layout,
        vk::ImageLayout new_layout,
        vk::AccessFlags2 src_access_mask,
        vk::AccessFlags2 dst_access_mask,
        vk::PipelineStageFlags2 src_stage_mask,
        vk::PipelineStageFlags2 dst_stage_mask,
        vk::ImageAspectFlags aspect_mask) const;

    void create_swapchain();
    void create_imageviews();

    vk::Format find_supported_format(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) const;
    inline vk::Format find_depth_format() const {
        return find_supported_format({vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint}, vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment);
    }

    std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> create_buffer(vk::DeviceSize size,
        vk::BufferUsageFlags usage,
        vk::MemoryPropertyFlags properties) const;

    vk::raii::CommandBuffer begin_single_commands() const;
    void end_single_commands(vk::raii::CommandBuffer&& cmd_buf) const;
    void copy_buffer(vk::raii::Buffer& src, vk::raii::Buffer& dst, vk::DeviceSize size) const;
    std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> load_into_staging_buffer(void* data, uint32_t size) const;

    template <vk::BufferUsageFlagBits buf_type>
    void create_input_attr_buffer(const float* src, vk::raii::Buffer& buf, vk::raii::DeviceMemory& memo,
        size_t comp_size, size_t elem_cnt) const
    {
        vk::DeviceSize buf_size = comp_size * elem_cnt;
        auto [staging_buf, staging_memo] = create_buffer(buf_size, vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        void* data = staging_memo.mapMemory(0, buf_size);
        std::memcpy(data, src, static_cast<size_t>(buf_size));
        staging_memo.unmapMemory();
        std::tie(buf, memo) = create_buffer(buf_size, vk::BufferUsageFlagBits::eTransferDst | buf_type,
            vk::MemoryPropertyFlagBits::eDeviceLocal);
        copy_buffer(staging_buf, buf, buf_size);
    }

    uint32_t find_memory_type(uint32_t type_filter, vk::MemoryPropertyFlags properties) const;

    void create_depth_resources();

    void transit_image_layout(vk::raii::CommandBuffer& cmd_buf, const vk::raii::Image& img, vk::ImageLayout old_layout, vk::ImageLayout new_layout, uint32_t layer_count = 1) const;
    void copy_buffer_to_image(vk::raii::CommandBuffer& cmd_buf, const vk::raii::Buffer& buf, const vk::raii::Image& img, uint32_t width, uint32_t height) const;
    std::pair<vk::raii::Image, vk::raii::DeviceMemory> create_vk_image(uint32_t width, uint32_t height, uint32_t layers, vk::SampleCountFlagBits samples, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::ImageCreateFlags flags = {}) const;
    vk::raii::ImageView create_vk_imageview(vk::Image img, vk::Format format, vk::ImageAspectFlags aspect_mask = vk::ImageAspectFlagBits::eColor) const;
    vk::raii::ImageView create_vk_imageview(const vk::raii::Image& img, vk::Format format, uint32_t layer_count = 1,
        vk::ImageAspectFlags aspect_mask = vk::ImageAspectFlagBits::eColor) const;
    vk::raii::Sampler create_vk_sampler(vk::Filter mag_filter=vk::Filter::eLinear,
        vk::Filter min_filter=vk::Filter::eLinear,
        vk::SamplerMipmapMode mipmap_mode=vk::SamplerMipmapMode::eLinear,
        vk::SamplerAddressMode address_mode=vk::SamplerAddressMode::eRepeat,
        bool anisotropy_enable=vk::True,
        bool compare_enable=vk::False,
        vk::CompareOp compare_op=vk::CompareOp::eAlways) const;

private:
    std::vector<const char*> required_extensions = {
        vk::KHRSwapchainExtensionName
    };

    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;
    vk::raii::DebugUtilsMessengerEXT debug_messenger = nullptr;

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

    vk::raii::Image depth_image = nullptr;
    vk::raii::DeviceMemory depth_memo = nullptr;
    vk::raii::ImageView depth_view = nullptr;

    vk::raii::CommandPool command_pool = nullptr;

    std::vector<vk::raii::Semaphore> image_available_semaphores;
    std::vector<vk::raii::Semaphore> render_finished_semaphores;
    std::vector<vk::raii::Fence> in_flight_fences;
    std::vector<vk::Fence> images_in_flight;

    uint32_t current_frame = 0;
    GLFWwindow* window_ = nullptr;
    UpdateCallback update_cbk_;
    std::chrono::steady_clock::time_point last_frame_time_ = std::chrono::steady_clock::now();
    bool enable_debug_messenger = true;

public:
    std::unordered_map<std::string, Pipeline> pipelines;
    std::unordered_map<std::string, MeshGPU> meshes;
    std::vector<vk::raii::CommandBuffer> command_buffers;
    bool frame_buffer_resized = false;

    std::unordered_map<std::string, UBO> ubos;
    std::unordered_map<std::string, Texture> textures;
};

}