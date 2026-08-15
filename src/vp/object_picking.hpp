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

struct ObjectPickingInstance {
    glm::mat4 model{1.0f};
    uint32_t object_id = 0;
    uint32_t pad0 = 0;
    uint32_t pad1 = 0;
    uint32_t pad2 = 0;
};

class ObjectPickingFeature final : public ViewportFeature<ViewportPhase::Picking> {
public:
    explicit ObjectPickingFeature(const Camera& camera);

    void on_attach(Context& context, vk::Extent2D extent);
    void on_resize(Context& context, vk::Extent2D extent);
    void on_update(Context& context, const Context::Frame& frame);
    void on_record(Context& context, vk::raii::CommandBuffer& cmd, uint32_t image_index);

    void add_object(std::string mesh_name, uint32_t object_id, glm::mat4 model = glm::mat4{1.0f});
    void clear_objects();
    void set_pick_callback(std::function<void(uint32_t)> callback);
    uint32_t picked_object_id() const { return picked_id; }

    bool enabled = true;

private:
    bool create_pipeline(Context& context);
    void update_instance_buffer(Context& context);

    const Camera& camera;
    std::vector<ObjectPickingInstance> instances;
    std::vector<std::string> mesh_names;
    std::function<void(uint32_t)> pick_callback;
    uint32_t target_index = kInvalidTargetIndex;
    uint32_t picked_id = 0;
    uint32_t pending_x = 0;
    uint32_t pending_y = 0;
    uint64_t pending_serial = 0;
    uint64_t last_render_serial = 0;
    uint64_t current_serial = 0;
    size_t allocated_instance_count = 0;
    bool mouse_down = false;
    bool pick_pending = false;
    bool instance_buffer_dirty = true;
    bool ready = false;
};

} // namespace vkkk::vp
