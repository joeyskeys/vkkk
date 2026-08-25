#pragma once

#include <array>
#include <cstdint>

#include "built_in_shader/fixed_color.h"
#include "concepts/camera.h"
#include "vp/feature.hpp"

namespace vkkk::vp
{

// World-space XZ grid centered on the origin.
class GridFeature final : public ViewportFeature<ViewportPhase::Scene> {
public:
    explicit GridFeature(const Camera& camera, uint32_t cell_count = 24, float cell_size = 1.0f);

    void on_attach(Context& context, vk::Extent2D extent);
    void on_update(Context& context, const Context::Frame& frame);
    void on_record(Context& context, vk::raii::CommandBuffer& cmd, uint32_t image_index);

    bool visible = true;

private:
    const Camera& camera;
    uint32_t cell_count;
    float cell_size;
    CameraUBO camera_ubo{};
    std::array<FixedColorInstanceAttrs, 2> instances{};
    bool ready = false;
    bool wide_lines = false;
};

} // namespace vkkk::vp
