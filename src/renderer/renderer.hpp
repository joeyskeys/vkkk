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

public:
    Context* ctx = nullptr;
    Scene* scene = nullptr;
    Camera* camera = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
};

} // namespace vkkk
