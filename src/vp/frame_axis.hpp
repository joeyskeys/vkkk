#pragma once

#include <array>

#include "built_in_shader/fixed_color.h"
#include "concepts/camera.h"
#include "vp/feature.hpp"

namespace vkkk::vp
{

// Screen-corner orientation gizmo rendered as red, green, and blue strokes.
class FrameAxisFeature final : public ViewportFeature<ViewportPhase::ScreenOverlay> {
public:
    explicit FrameAxisFeature(const Camera& camera);

    void on_attach(Context& context, vk::Extent2D extent);
    void on_update(Context& context, const Context::Frame& frame);
    void on_record(Context& context, vk::raii::CommandBuffer& cmd, uint32_t image_index);

    bool visible = true;

private:
    const Camera& camera_;
    CameraUBO overlay_camera_{};
    std::array<FixedColorInstanceAttrs, 3> instances_{};
    bool ready = false;
};

} // namespace vkkk::vp
