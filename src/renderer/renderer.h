#pragma once

#include <cstdint>
#include <string>

namespace vkkk
{

class Scene;
class VkWrappedInstance;

struct RenderView {
    uint32_t swapchain_image_idx = 0;
    float delta_seconds = 0.0f;
};

class Renderer {
public:
    virtual ~Renderer() = default;

    virtual const char* type_name() const = 0;

    // Bind renderer lifetime to an already initialized Vulkan instance.
    virtual bool initialize(VkWrappedInstance* instance) = 0;
    virtual void shutdown() = 0;

    // Scene data to be consumed by concrete renderer implementation.
    virtual void set_scene(Scene* scene) = 0;

    // Called once per frame before command buffer submission.
    virtual void render(const RenderView& view) = 0;

    // Called when swapchain extent or render targets change.
    virtual void on_resize(uint32_t width, uint32_t height) = 0;
};

} // namespace vkkk
