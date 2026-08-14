#pragma once

#include <array>
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

#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "concepts/line.h"
#include "concepts/mesh.h"
#include "vk_ins/compute_shader.hpp"
#include "vk_ins/shader_module_pack.hpp"
#include "vk_ins/types.h"
#include "built_in_shader/common.h"

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

// We have a design division here. ubo is actually bound to pipeline, the GPU part.
// But the CPU part is actually not, it can be included here as a singleton here, or
// helded with multiple copies by objs which might use the pipeline to draw. The
// two different design means whether or not you should update entire uniform buffer
// each time.
// And especially, if the uniform members have logic relationships, or time related,
// then you ultimately need multi-copies anyway.
struct UBO {
    size_t                                  size;
    size_t                                  vecsize;
    uint32_t                                binding;
    vk::DescriptorType                      descriptor_type = vk::DescriptorType::eUniformBuffer;
    //std::shared_ptr<char[]>                 cpu_buf;
    std::vector<vk::raii::Buffer>           gpu_bufs;
    std::vector<vk::raii::DeviceMemory>     memos;
    std::vector<vk::DescriptorBufferInfo>   descriptors;
};

struct SSBO : public UBO {
    std::vector<vk::DescriptorBufferInfo> borrowed_descriptors;
    bool uses_borrowed_descriptors = false;
};

struct Texture {
    uint32_t                                binding;
    size_t                                  vecsize;
    uint32_t                                width = 0;
    uint32_t                                height = 0;
    vk::Format                              format = vk::Format::eUndefined;
    vk::ImageUsageFlags                     usage{};
    vk::SampleCountFlagBits                 samples = vk::SampleCountFlagBits::e1;
    // True when created with width/height 0 (follow swapchain extent on resize).
    bool                                    matchSwapchain = false;
    vk::raii::Image                         image{nullptr};
    vk::raii::DeviceMemory                  memo{nullptr};
    vk::raii::ImageView                     view{nullptr};
    vk::ImageLayout                         layout;
    vk::DescriptorImageInfo                 descriptor;
    vk::raii::Sampler                       sampler{nullptr};
};

// PassDesc target indices: swapchain color, default/no depth.
inline constexpr int32_t kSwapchainTarget = -1;
inline constexpr int32_t kNoDepth = -1;
inline constexpr int32_t kDefaultDepth = -2;
inline constexpr uint32_t kInvalidTargetIndex = ~0u;

enum class PassLoadOp : uint8_t { Clear, Load, DontCare };
enum class PassStoreOp : uint8_t { Store, DontCare };

struct ColorTargetRef {
    int32_t target_index = kSwapchainTarget;
    PassLoadOp load = PassLoadOp::Clear;
    PassStoreOp store = PassStoreOp::Store;
    std::array<float, 4> clear = {0.f, 0.f, 0.f, 1.f};
};

struct PassDesc {
    std::vector<ColorTargetRef> colors = { ColorTargetRef{} };
    // kDefaultDepth = Context depth; kNoDepth = none; >=0 = depth_attachments[i]
    int32_t depth_index = kDefaultDepth;
    PassLoadOp depth_load = PassLoadOp::Clear;
    PassStoreOp depth_store = PassStoreOp::Store;
    float depth_clear = 1.f;
    // Transition swapchain color targets to PresentSrc after the pass.
    bool present = true;
};

struct DepthAttachment {
    uint32_t                                width = 0;
    uint32_t                                height = 0;
    vk::Format                              format = vk::Format::eUndefined;
    vk::ImageAspectFlags                    aspect_mask = vk::ImageAspectFlagBits::eDepth;
    vk::SampleCountFlagBits                 samples = vk::SampleCountFlagBits::e1;
    // True when created with width/height 0 (follow swapchain extent on resize).
    bool                                    matchSwapchain = false;
    vk::raii::Image                         image{nullptr};
    vk::raii::DeviceMemory                  memo{nullptr};
    // Single depth-aspect view: used both as depth attachment and for sampling.
    // Created with a depth-only format so one view is valid for both purposes.
    vk::raii::ImageView                     view{nullptr};
    vk::raii::Sampler                       sampler{nullptr};
    vk::ImageLayout                         layout = vk::ImageLayout::eUndefined;
    vk::DescriptorImageInfo                 descriptor{};
    bool                                    initialized = false;
};

// Reflected push-constant block with CPU staging storage.
struct PushConstantBlock {
    uint32_t size = 0;
    uint32_t offset = 0;
    vk::ShaderStageFlags stages{};
    std::vector<char> data;
};

struct Pipeline {
    vk::raii::Pipeline vk_pipeline{nullptr};
    vk::raii::PipelineLayout vk_pipeline_layout{nullptr};
    vk::raii::DescriptorSetLayout descriptor_set_layout{nullptr};
    vk::raii::DescriptorPool descriptor_pool{nullptr};
    std::vector<vk::raii::DescriptorSet> descriptor_sets;
    bool uses_mesh_shader = false;

    // Keys are reflected GLSL block/type names from SPIR-V.
    std::unordered_map<std::string, UBO> ubos;
    std::unordered_map<std::string, SSBO> ssbos;
    std::unordered_map<std::string, PushConstantBlock> push_constants;
    std::unordered_map<uint32_t, uint32_t> sampler_descriptor_counts;
};

struct ComputePipeline {
    vk::raii::Pipeline vk_pipeline{nullptr};
    vk::raii::PipelineLayout vk_pipeline_layout{nullptr};
    vk::raii::DescriptorSetLayout descriptor_set_layout{nullptr};
    vk::raii::DescriptorPool descriptor_pool{nullptr};
    std::vector<vk::raii::DescriptorSet> descriptor_sets;
    std::unordered_map<std::string, UBO> ubos;
    std::unordered_map<std::string, SSBO> ssbos;
    std::unordered_map<std::string, PushConstantBlock> push_constants;
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

struct MeshGPU {
    vk::raii::Buffer                        vbuf{nullptr};
    vk::raii::DeviceMemory                  vbuf_memo{nullptr};
    vk::raii::Buffer                        ibuf{nullptr};
    vk::raii::DeviceMemory                  ibuf_memo{nullptr};
    uint32_t                                vcnt = 0;
    uint32_t                                icnt = 0;
    vk::DeviceSize                          vert_bytes = 0;
    vk::DeviceSize                          index_bytes = 0;
    
    void sync(const Mesh& mesh, class Context* ctx);
    void emit_draw_cmd(vk::CommandBuffer cmd_buf, vk::PipelineLayout ppl_layout,
        const vk::DescriptorSet* desc_set = nullptr, uint32_t index_count = 0) const;
    void emit_draw_cmd_instanced(vk::CommandBuffer cmd_buf, vk::PipelineLayout ppl_layout,
        uint32_t instance_count, uint32_t ssbo_offset, const vk::DescriptorSet* desc_set = nullptr,
        uint32_t index_count = 0) const;
};

struct LinesGPU {
    vk::raii::Buffer                        vbuf{nullptr};
    vk::raii::DeviceMemory                  vbuf_memo{nullptr};
    vk::raii::Buffer                        ibuf{nullptr};
    vk::raii::DeviceMemory                  ibuf_memo{nullptr};
    uint32_t                                vcnt = 0;
    uint32_t                                icnt = 0;
    vk::DeviceSize                          vert_bytes = 0;

    void sync(const Lines& lines, class Context* ctx);
    void emit_draw_cmd(vk::CommandBuffer cmd_buf, uint32_t index_count = 0,
        uint32_t instance_count = 1, uint32_t instance_offset = 0) const;
};

struct CameraGPU {
    uint32_t                                binding;
    vk::raii::Buffer                        buf{nullptr};
    vk::raii::DeviceMemory                  memo{nullptr};
    vk::DescriptorBufferInfo                descriptor;

    void sync(Camera& cam, Context* ctx) const;
};

// GPU buffer of VkDraw(Indexed)IndirectCommand, one copy per swapchain image.
struct IndirectBuffer {
    bool indexed = true;
    uint32_t command_capacity = 0;
    uint32_t command_stride = 0;
    std::vector<vk::raii::Buffer> gpu_bufs;
    std::vector<vk::raii::DeviceMemory> memos;
};

enum class BillboardTextSourceType : uint8_t {
    Texture,
    RenderTarget,
};

struct BillboardTextSource {
    BillboardTextSourceType type = BillboardTextSourceType::Texture;
    std::string texture_name;
    uint32_t target_index = kInvalidTargetIndex;

    static BillboardTextSource texture(const std::string& name) {
        return BillboardTextSource{BillboardTextSourceType::Texture, name, kInvalidTargetIndex};
    }
    static BillboardTextSource render_target(uint32_t index) {
        return BillboardTextSource{BillboardTextSourceType::RenderTarget, {}, index};
    }
};

struct BillboardTextVertex {
    glm::vec3 position{0.0f};
    glm::vec2 uv{0.0f};
};

struct BillboardTextOptions {
    glm::vec3 position{0.0f};
    glm::vec2 size{1.0f};
    bool depth_test = true;
};

struct BillboardTextData {
    glm::vec4 position{0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 size{1.0f, 1.0f, 1.0f, 0.0f};
};

// a class manages vulkan instance, physical device, logical device, surface
// these parts are not frequently changed or used.
class Context {
public:
    struct Frame {
        uint32_t image_index = ~0u;
        float dt = 0.0f;
        uint64_t serial = 0;
    };

    // Initialization, window, and swapchain
    Context(bool enable_debug_m = true);

    static std::vector<const char*> get_glfw_instance_extensions(bool enable_validation = true);
    GLFWwindow* init_glfw(int width, int height, const char* title = default_app_name, bool resizable = false);
    void init(GLFWwindow* win,
        const char* app_name = default_app_name,
        uint32_t app_version = default_app_version,
        const char* engine_name = default_engine_name,
        uint32_t api_version = default_api_version,
        bool enable_validation_layers = true,
        const std::vector<const char*>& extra_validation_layers = {},
        const std::vector<const char*>& extra_extensions = {});
    void recreate_swapchain();
    void wait_idle() const { device.waitIdle(); }

    // Pipeline creation
    bool create_pipeline(const std::string& name,
        const ShaderModulePack& shader_module_pack,
        const PipelineOption& option,
        const std::vector<VERT_COMP>& comps,
        bool interleaved = true,
        bool depth_only = false,
        const std::vector<vk::Format>& color_formats = {});
    bool create_compute_pipeline(const std::string& name, const ComputeShader& shader);
    bool load_compute_pipeline(const std::string& name, const fs::path& path);

    // Command recording, draw, and dispatch
    void dispatch_compute(const std::string& pipeline_name, uint32_t group_x, uint32_t group_y = 1,
        uint32_t group_z = 1, uint32_t descriptor_set_index = 0);
    bool record_compute(vk::CommandBuffer cmd, const std::string& pipeline_name,
        uint32_t group_x, uint32_t group_y = 1, uint32_t group_z = 1,
        uint32_t descriptor_set_index = 0) const;
    bool draw_mesh_tasks(vk::CommandBuffer cmd, uint32_t group_x, uint32_t group_y = 1,
        uint32_t group_z = 1) const;
    // Bind a mesh-shader graphics pipeline + descriptors, then draw mesh tasks.
    bool record_mesh_tasks(vk::CommandBuffer cmd, const std::string& pipeline_name,
        uint32_t group_x, uint32_t group_y = 1, uint32_t group_z = 1,
        uint32_t descriptor_set_index = 0) const;
    bool draw_mesh_instanced(vk::CommandBuffer cmd, const std::string& mesh_name,
        vk::PipelineLayout pipeline_layout, uint32_t instance_count, uint32_t ssbo_offset,
        const vk::DescriptorSet* desc_set = nullptr, uint32_t index_count = 0) const;
    // Reset/begin (or end) the per-swapchain command buffer.
    void begin_cmds(uint32_t image_index);
    void end_cmds(uint32_t image_index);
    // Dynamic rendering pass with load/store, MRT, and optional present.
    void begin_pass(vk::raii::CommandBuffer& cmd, uint32_t image_index, const PassDesc& pass);
    void end_pass(vk::raii::CommandBuffer& cmd, uint32_t image_index, const PassDesc& pass);
    // Convenience: begin_cmds + optional pre + default PassDesc + emit + end_cmds.
    void record_cmds(uint32_t image_index,
        const std::function<void(vk::raii::CommandBuffer&, uint32_t)>& emit_func,
        const std::function<void(vk::raii::CommandBuffer&, uint32_t)>& pre_render_func = {});
    // Depth-only dynamic rendering into a custom depth attachment (no color target).
    bool record_depth_pass(vk::raii::CommandBuffer& cmd, uint32_t attachment_index,
        const std::function<void(vk::raii::CommandBuffer&)>& emit_func);
    // Explicit frame lifecycle: acquire, record commands, then submit and present.
    // A Frame is valid only after begin_frame succeeds and until end_frame returns.
    bool begin_frame(Frame& frame);
    void record_frame(const Frame& frame,
        const std::function<void(vk::raii::CommandBuffer&, uint32_t)>& emit_func);
    void end_frame(const Frame& frame);
    // Legacy callback-based convenience API. Prefer begin_frame/record_frame/end_frame.
    void draw_frame();

    // GPU buffers and descriptors
    bool add_ubo(std::unordered_map<std::string, UBO>& ubos, const std::string& name, uint32_t binding,
        uint32_t size, uint32_t vecsize = 1,
        vk::BufferUsageFlags usage = vk::BufferUsageFlagBits::eUniformBuffer,
        vk::DescriptorType descriptor_type = vk::DescriptorType::eUniformBuffer);
    void sync_uniform(const vk::raii::DeviceMemory& memo, const void* data, uint32_t size) const;
    void sync_ssbo(const SSBO& ssbo, const void* data, uint32_t swapchain_idx, uint32_t byte_size = 0) const;
    // Sync by reflected GLSL block/type name (allocated from SPIR-V sizes at create_pipeline).
    bool sync_ubo(const std::string& pipeline_name, const std::string& block_name, const void* data,
        uint32_t frame_idx, uint32_t byte_size = 0) const;
    bool sync_ssbo(const std::string& pipeline_name, const std::string& block_name, const void* data,
        uint32_t frame_idx, uint32_t byte_size = 0) const;
    // Update CPU staging for a reflected push-constant block (does not record cmds).
    bool update_push_constants(const std::string& pipeline_name, const std::string& block_name,
        const void* data, uint32_t byte_size = 0);
    // Record vkCmdPushConstants. If data is non-null, updates staging first.
    bool push_constants(vk::CommandBuffer cmd, const std::string& pipeline_name,
        const std::string& block_name, const void* data = nullptr, uint32_t byte_size = 0);
    // Bind graphics pipeline + per-frame descriptor set.
    bool bind(vk::CommandBuffer cmd, const std::string& pipeline_name, uint32_t frame_idx) const;
    // Draw a named mesh. Call bind() first for the same pipeline/frame.
    bool draw(vk::CommandBuffer cmd, const std::string& pipeline_name, const std::string& mesh_name,
        uint32_t instance_count, uint32_t instance_offset = 0, uint32_t index_count = 0) const;
    // Draw a named mesh with an explicit index count. Call bind() first for the intended pipeline/frame.
    bool draw_indexed(vk::CommandBuffer cmd, const std::string& mesh_name, uint32_t index_count,
        uint32_t instance_count = 1, uint32_t instance_offset = 0) const;
    // Draw named indexed line-list data. Call bind() first with an eLineList pipeline.
    // index_count 0 draws the complete line index buffer.
    bool draw_lines(vk::CommandBuffer cmd, const std::string& lines_name,
        uint32_t index_count = 0, uint32_t instance_count = 1,
        uint32_t instance_offset = 0) const;
    // Create/resize a named indirect-command buffer (per swapchain image).
    bool create_indirect_buffer(const std::string& name, uint32_t command_capacity,
        bool indexed = true);
    bool resize_indirect_buffer(const std::string& name, uint32_t command_capacity);
    // Upload Draw(Indexed)IndirectCommand array for one frame. command_count 0 = capacity.
    bool sync_indirect_buffer(const std::string& name, const void* data, uint32_t frame_idx,
        uint32_t command_count = 0) const;
    // Bind mesh VB/IB then vkCmdDraw(Indexed)Indirect. Call bind() first.
    // draw_count 0 = command_capacity; first_draw indexes into the command array.
    bool draw_indirect(vk::CommandBuffer cmd, const std::string& mesh_name,
        const std::string& indirect_name, uint32_t frame_idx, uint32_t draw_count = 0,
        uint32_t first_draw = 0) const;
    bool alloc_pipeline_ssbo(const std::string& pipeline_name, const std::string& block_name);
    bool resize_pipeline_ssbo(const std::string& pipeline_name, const std::string& block_name,
        size_t new_vecsize);
    bool bind_pipeline_ssbo_from_compute(const std::string& graphics_pipeline_name,
        const std::string& graphics_block_name, const std::string& compute_pipeline_name,
        const std::string& compute_block_name);
    // Bind a sampleable render target to a reflected combined-image-sampler binding.
    bool bind_pipeline_render_target(const std::string& pipeline_name, uint32_t binding,
        uint32_t target_index);
    // Bind a named 2D texture to a reflected combined-image-sampler binding.
    bool bind_pipeline_texture(const std::string& pipeline_name, uint32_t binding,
        const std::string& texture_name);
    // Bind a sampleable depth attachment as a combined comparison sampler (e.g. sampler2DShadow).
    bool bind_pipeline_depth_attachment(const std::string& pipeline_name, uint32_t binding,
        uint32_t attachment_index);
    // Bind MeshGPU vertex/index buffers as pipeline Vertices/Indices SSBOs for mesh shaders.
    bool bind_pipeline_ssbo_from_mesh(const std::string& pipeline_name, const std::string& mesh_name);
    bool alloc_compute_ssbo(const std::string& full_name);
    bool resize_compute_ssbo(const std::string& full_name, size_t new_vecsize);
    bool sync_compute_ssbo(const std::string& full_name, const void* data,
        uint32_t swapchain_idx, uint32_t byte_size = 0) const;
    bool alloc_compute_ssbo(const std::string& pipeline_name, const std::string& block_name);
    bool resize_compute_ssbo(const std::string& pipeline_name, const std::string& block_name,
        size_t new_vecsize);
    bool sync_compute_ssbo(const std::string& pipeline_name, const std::string& block_name,
        const void* data, uint32_t swapchain_idx, uint32_t byte_size = 0) const;
    UBO& require_ubo(const std::string& full_name);
    const SSBO& require_compute_ssbo(const std::string& full_name) const;

    // Textures, render targets, and depth attachments
    bool add_texture(const std::string& name, uint32_t binding,
        const fs::path& path);
    bool add_cubemap(const std::string& name, uint32_t binding,
        const fs::path& path);
    // Returns the new target index, or kInvalidTargetIndex on failure.
    uint32_t add_render_target(vk::ImageUsageFlags usage, vk::Format format,
        uint32_t width = 0, uint32_t height = 0,
        vk::ImageLayout layout = vk::ImageLayout::eGeneral,
        vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1);
    // Create a sampled RGBA8 render target populated from CPU pixels.
    uint32_t create_rgba8_render_target(const uint8_t* pixels, uint32_t width, uint32_t height,
        size_t byte_size);
    bool resize_render_target(uint32_t target_index, uint32_t width, uint32_t height);
    bool set_render_target(uint32_t target_index);
    void set_render_to_framebuffer();
    // Sampleable depth attachment (shadow maps, etc.). Defaults to a depth-only format
    // so a single ImageView can be used for both writing and sampling.
    bool add_depth_attachment(uint32_t width = 0, uint32_t height = 0,
        vk::Format format = vk::Format::eUndefined,
        vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1);
    bool resize_depth_attachment(uint32_t attachment_index, uint32_t width, uint32_t height);
    bool set_depth_attachment(uint32_t attachment_index);
    void set_default_depth_attachment();

    // Mesh resources
    bool load_mesh(const std::string& name, const Mesh& mesh);
    bool load_lines(const std::string& name, const Lines& lines);
    // Add a camera-facing textured billboard using an auto-generated indexed quad.
    bool add_billboard_text(const std::string& name, const BillboardTextSource& source,
        const BillboardTextOptions& options = {});
    // Add a camera-facing textured billboard using caller-provided local quad vertices.
    bool add_billboard_text_quad(const std::string& name,
        const std::array<BillboardTextVertex, 4>& vertices, const BillboardTextSource& source,
        const BillboardTextOptions& options = {});
    bool set_billboard_text_transform(const std::string& name, const glm::vec3& position,
        const glm::vec2& size);
    // Bind and draw one billboard. The caller owns pass lifetime and supplies the active camera.
    bool draw_billboard_text(vk::CommandBuffer cmd, const std::string& name,
        const CameraUBO& camera, uint32_t frame_idx);
    void create_vertex_buffer(const float* src, vk::raii::Buffer& buf, vk::raii::DeviceMemory& memo,
        size_t comp_size, size_t vcnt) const
    {
        create_input_attr_buffer(
            src, buf, memo, comp_size * sizeof(float), vcnt,
            vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer);
    }

    void create_index_buffer(const uint32_t* src, vk::raii::Buffer& buf, vk::raii::DeviceMemory& memo,
        size_t idx_cnt) const
    {
        create_input_attr_buffer(
            reinterpret_cast<const float*>(src), buf, memo, sizeof(uint32_t), idx_cnt,
            vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eStorageBuffer);
    }

    // Accessors and configuration
    GLFWwindow* get_window() const { return window; }
    VkInstance get_vk_instance() const { return static_cast<VkInstance>(*instance); }
    VkPhysicalDevice get_vk_physical_device() const { return static_cast<VkPhysicalDevice>(*physical_device); }
    VkDevice get_vk_device() const { return static_cast<VkDevice>(*device); }
    VkQueue get_vk_queue() const { return static_cast<VkQueue>(*queue); }
    VkQueue get_vk_compute_queue() const { return static_cast<VkQueue>(*compute_queue); }
    uint32_t get_graphic_queue_family_index() const { return queue_idx; }
    uint32_t get_compute_queue_family_index() const { return compute_queue_idx; }
    uint32_t get_swapchain_count() const { return static_cast<uint32_t>(swapchain_images.size()); }
    VkFormat get_swapchain_format() const { return static_cast<VkFormat>(swapchain_surface_format.format); }
    VkFormat get_depth_format() const { return static_cast<VkFormat>(find_depth_format()); }
    vk::raii::CommandBuffer begin_single_commands() const;
    void end_single_commands(vk::raii::CommandBuffer&& cmd_buf) const;

    using UpdateCallback = std::function<void(uint32_t image_index, float dt)>;
    using ResizeCallback = std::function<void(uint32_t width, uint32_t height)>;
    void set_update_cbk(UpdateCallback cbk) { update_cbk_ = std::move(cbk); }
    void set_resize_cbk(ResizeCallback cbk) { resize_cbk_ = std::move(cbk); }
    vk::Extent2D extent() const { return swapchain_extent; }

    // Global runtime toggle: try to use mesh shaders for compatible pipelines.
    // When true, Context enables VK_EXT_mesh_shader + required feature bits during init.
    bool use_mesh_shader = true;
    vk::SampleCountFlagBits nsample = vk::SampleCountFlagBits::e1;

private:
    // Internal device and resource helpers
    void setup_debug_messenger();
    static bool is_device_suitable(
        const vk::raii::PhysicalDevice& device,
        vk::raii::SurfaceKHR& surface,
        const std::vector<const char*>& required_extensions);
    static uint32_t choose_min_image_count(const vk::SurfaceCapabilitiesKHR& surface_capabilities);
    static vk::SurfaceFormatKHR choose_swap_surface_format(
        const std::vector<vk::SurfaceFormatKHR>& surface_formats);
    static vk::PresentModeKHR choose_present_mode(const std::vector<vk::PresentModeKHR>& present_modes);
    static vk::Extent2D choose_swap_extent(
        const vk::SurfaceCapabilitiesKHR& surface_capabilities,
        GLFWwindow* window);
    uint32_t find_graphics_queue_family_index() const;
    uint32_t find_compute_queue_family_index(uint32_t preferred_graphics_index) const;
    static vk::DescriptorType to_vk_descriptor_type(ComputeDescriptorKind kind);
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
        return find_supported_format({vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint, vk::Format::eD32Sfloat}, vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment);
    }
    // Depth-only formats used by sampleable DepthAttachment / depth-only pipelines.
    inline vk::Format find_depth_only_format() const {
        return find_supported_format({vk::Format::eD32Sfloat, vk::Format::eD16Unorm},
            vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment);
    }

    std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> create_buffer(vk::DeviceSize size,
        vk::BufferUsageFlags usage,
        vk::MemoryPropertyFlags properties) const;

    void copy_buffer(vk::raii::Buffer& src, vk::raii::Buffer& dst, vk::DeviceSize size) const;
    std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> load_into_staging_buffer(void* data, uint32_t size) const;
    vk::raii::CommandBuffer begin_single_commands(const vk::raii::CommandPool& pool) const;
    void end_single_commands(vk::raii::CommandBuffer&& cmd_buf, const vk::raii::Queue& submit_queue) const;

    void create_input_attr_buffer(const float* src, vk::raii::Buffer& buf, vk::raii::DeviceMemory& memo,
        size_t elem_stride_bytes, size_t elem_cnt, vk::BufferUsageFlags usage) const
    {
        vk::DeviceSize buf_size = elem_stride_bytes * elem_cnt;
        auto [staging_buf, staging_memo] = create_buffer(buf_size, vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        void* data = staging_memo.mapMemory(0, buf_size);
        std::memcpy(data, src, static_cast<size_t>(buf_size));
        staging_memo.unmapMemory();
        std::tie(buf, memo) = create_buffer(buf_size, vk::BufferUsageFlagBits::eTransferDst | usage,
            vk::MemoryPropertyFlagBits::eDeviceLocal);
        copy_buffer(staging_buf, buf, buf_size);
    }

    uint32_t find_memory_type(uint32_t type_filter, vk::MemoryPropertyFlags properties) const;

    void create_depth_resources();

    bool create_pipeline_ssbo_gpu(Pipeline& pipeline, SSBO& ssbo);
    void update_pipeline_ssbo_descriptors(Pipeline& pipeline, const SSBO& ssbo);
    bool create_compute_ssbo_gpu(ComputePipeline& pipeline, SSBO& ssbo);
    void update_compute_ssbo_descriptors(ComputePipeline& pipeline, const SSBO& ssbo);

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
    vk::raii::Queue compute_queue = nullptr;
    uint32_t queue_idx = ~0;
    uint32_t compute_queue_idx = ~0;

    vk::raii::SwapchainKHR swapchain = nullptr;
    std::vector<vk::Image> swapchain_images;
    std::vector<vk::raii::ImageView> swapchain_image_views;
    std::vector<vk::ImageLayout> swapchain_image_layouts;
    vk::Extent2D swapchain_extent;
    vk::SurfaceFormatKHR swapchain_surface_format;

    vk::raii::Image depth_image = nullptr;
    vk::raii::DeviceMemory depth_memo = nullptr;
    vk::raii::ImageView depth_view = nullptr;

    vk::raii::CommandPool command_pool = nullptr;
    vk::raii::CommandPool compute_command_pool = nullptr;

    std::vector<vk::raii::Semaphore> image_available_semaphores;
    std::vector<vk::raii::Semaphore> render_finished_semaphores;
    std::vector<vk::raii::Fence> in_flight_fences;
    std::vector<vk::Fence> images_in_flight;

    uint32_t current_frame = 0;
    GLFWwindow* window = nullptr;
    UpdateCallback update_cbk_;
    ResizeCallback resize_cbk_;
    std::chrono::steady_clock::time_point last_frame_time_ = std::chrono::steady_clock::now();
    bool frame_active = false;
    bool frame_recorded = false;
    uint32_t active_frame_image_index = ~0u;
    uint64_t active_frame_serial = 0;
    uint64_t next_frame_serial = 1;
    bool enable_debug_messenger = true;
    bool mesh_shader_available = false;
    bool task_shader_available = false;
    bool depth_image_initialized = false;
    int32_t active_render_target_index_ = -1;
    int32_t active_depth_attachment_index_ = -1;

    // Pipelines that sample a custom color RT or depth attachment (rebind after resize).
    struct SampledAttachmentBind {
        std::string pipeline_name;
        uint32_t binding = 0;
        uint32_t attachment_index = 0;
        bool is_depth = false;
    };
    std::vector<SampledAttachmentBind> sampled_attachment_binds;
    struct BillboardText {
        std::string pipeline_name;
        std::string mesh_name;
        BillboardTextOptions options;
    };
    std::unordered_map<std::string, BillboardText> billboard_texts;

public:
    std::unordered_map<std::string, Pipeline> pipelines;
    std::unordered_map<std::string, ComputePipeline> compute_pipelines;
    std::unordered_map<std::string, MeshGPU> meshes;
    std::unordered_map<std::string, LinesGPU> lines;
    std::unordered_map<std::string, IndirectBuffer> indirect_buffers;
    std::vector<vk::raii::CommandBuffer> command_buffers;
    bool frame_buffer_resized = false;
    bool sample_rate_shading_enabled = false;
    // True when VK_FEATURE_WIDE_LINES was supported and enabled at device creation.
    bool wide_lines_enabled = false;

    std::unordered_map<std::string, Texture> textures;
    std::vector<Texture> targets;
    std::vector<DepthAttachment> depth_attachments;
};

}