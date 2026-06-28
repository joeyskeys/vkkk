#pragma once

#include <cstdint>
#include <string>

#include <vulkan/vulkan.h>

namespace vkkk
{

class Scene;
class Camera;
class Context;

struct RenderView {
    uint32_t swapchain_image_idx = 0;
    float delta_seconds = 0.0f;
};

class Renderer {
public:
    virtual ~Renderer() = default;

    virtual const char* type_name() const = 0;

    // Bind renderer lifetime to an already initialized Vulkan instance.
    virtual bool initialize(Context* context) = 0;
    virtual void shutdown() = 0;

    // Scene data to be consumed by concrete renderer implementation.
    virtual void set_scene(Scene* s) { scene = s; }

    // Camera data to be consumed by concrete renderer implementation.
    virtual void set_camera(Camera* c) { camera = c; }

    // Called once per frame before command buffer submission.
    virtual void update(const RenderView& view) = 0;

    // Record draw commands into the active swapchain command buffer.
    virtual void record_commands(vk::CommandBuffer cmd, const RenderView& view) {}

    // Called when swapchain extent or render targets change.
    virtual void on_resize(uint32_t width, uint32_t height) = 0;

    // utils
    bool create_pipeline_from_shader_src(const string& ppl_name,
        const char* vert_or_mesh,
        const char* frag,
        const PipelineOption& option);

    void sync_uniforms(const uint32_t swapchain_idx,
        const InstanceAttr& instance_attr,
        const std::string& pipeline_name);
    void sync_ssbos(const uint32_t swapchain_idx,
        const InstanceAttr& instance_attr,
        const Pipeline& pipeline);

public:
    Context* ctx = nullptr;
    Scene* scene = nullptr;
    Camera* camera = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    bool update_ssbo = false;

    // draw info: mesh name & instance attributes
    using DrawInfos = std::unordered_map<std::string, std::vector<InstanceAttr>>;
    // batch: value: pipeline name, key: draw infos
    using Batch = std::unordered_map<std::string, DrawInfos>;
};

} // namespace vkkk
