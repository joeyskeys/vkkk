#pragma once

#include <array>
#include <filesystem>
#include <string>

#include "built_in_shader/fixed_color.h"
#include "concepts/camera.h"
#include "vp/feature.hpp"

namespace vkkk::vp
{

// World-space directions and labels rendered by FrameAxisFeature. The default
// is the current right-handed Y-up frame: +X, +Y, +Z. This affects only the
// gizmo; callers using another convention must supply matching camera/shader
// transforms for their scene.
struct CoordinateSystem {
    std::array<glm::vec3, 3> axes = {
        glm::vec3{1.0f, 0.0f, 0.0f},
        glm::vec3{0.0f, 1.0f, 0.0f},
        glm::vec3{0.0f, 0.0f, 1.0f},
    };
    std::array<std::string, 3> labels = {"x", "y", "z"};
};

// Screen-corner orientation gizmo rendered as red, green, and blue strokes.
class FrameAxisFeature final : public ViewportFeature<ViewportPhase::ScreenOverlay> {
public:
    FrameAxisFeature(const Camera& camera, std::filesystem::path font_path = {},
        CoordinateSystem coordinate_system = {});

    void on_attach(Context& context, vk::Extent2D extent);
    void on_update(Context& context, const Context::Frame& frame);
    void on_record(Context& context, vk::raii::CommandBuffer& cmd, uint32_t image_index);

    bool visible = true;

private:
    const Camera& camera;
    std::filesystem::path font_path;
    CoordinateSystem coordinate_system;
    CameraUBO overlay_camera{};
    std::array<FixedColorInstanceAttrs, 3> instances{};
    std::array<float, 3> label_aspects{};
    bool ready = false;
    bool labels_ready = false;
};

} // namespace vkkk::vp
