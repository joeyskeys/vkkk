#pragma once

#include <array>
#include <filesystem>

#include "built_in_shader/fixed_color.h"
#include "concepts/camera.h"
#include "vp/feature.hpp"

namespace vkkk::vp
{

// Screen-corner orientation gizmo rendered as red, green, and blue strokes.
class FrameAxisFeature final : public ViewportFeature<ViewportPhase::ScreenOverlay> {
public:
    FrameAxisFeature(const Camera& camera, std::filesystem::path font_path = {});

    void on_attach(Context& context, vk::Extent2D extent);
    void on_update(Context& context, const Context::Frame& frame);
    void on_record(Context& context, vk::raii::CommandBuffer& cmd, uint32_t image_index);

    bool visible = true;

private:
    const Camera& camera;
    std::filesystem::path font_path;
    CameraUBO overlay_camera{};
    std::array<FixedColorInstanceAttrs, 3> instances{};
    std::array<glm::vec2, 3> label_sizes{};
    bool ready = false;
    bool labels_ready = false;
};

} // namespace vkkk::vp
