#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <glm/mat4x4.hpp>

#include "concepts/camera.h"
#include "vp/feature.hpp"

namespace vkkk::vp
{

struct VertexPickingParams {
    glm::mat4 model{1.0f};
    float point_size = 12.0f;
};

class VertexPickingFeature final : public ViewportFeature<ViewportPhase::Picking> {
public:
    explicit VertexPickingFeature(const Camera& camera, uint32_t nodes_per_pixel = 4);

    void on_attach(Context& context, vk::Extent2D extent);
    void on_resize(Context& context, vk::Extent2D extent);
    void on_update(Context& context, const Context::Frame& frame);
    void on_record(Context& context, vk::raii::CommandBuffer& cmd, uint32_t image_index);

    void set_point_list(std::string points_name, glm::mat4 model = glm::mat4{1.0f});
    void set_point_transform(glm::mat4 model);
    void clear_point_list();
    void set_pick_callback(std::function<void(const std::vector<uint32_t>&, bool)> callback);
    const std::vector<uint32_t>& picked_vertex_ids() const { return picked_ids; }
    bool overflowed() const { return overflow; }

    float point_size = 12.0f;
    bool enabled = true;

private:
    bool create_pipeline(Context& context);
    bool resize_buffers(Context& context, vk::Extent2D extent);

    const Camera& camera;
    std::string points_name;
    glm::mat4 model{1.0f};
    std::vector<uint32_t> picked_ids;
    std::function<void(const std::vector<uint32_t>&, bool)> pick_callback;
    uint32_t nodes_per_pixel = 4;
    uint32_t pending_x = 0;
    uint32_t pending_y = 0;
    uint32_t last_image_index = 0;
    uint64_t pending_serial = 0;
    uint64_t last_render_serial = 0;
    uint64_t current_serial = 0;
    bool mouse_down = false;
    bool pick_pending = false;
    bool overflow = false;
    bool ready = false;
};

} // namespace vkkk::vp
