#include "vp/object_picking.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <utility>

#include <GLFW/glfw3.h>

#include "built_in_shader/common.h"
#include "vk_ins/shader_module_pack.hpp"

namespace vkkk::vp
{

namespace
{

constexpr const char* kPipelineName = "vp_object_picking";

std::filesystem::path find_shader(const char* filename) {
    constexpr std::array<const char*, 5> prefixes = {
        "resource/shaders/", "../resource/shaders/", "../../resource/shaders/",
        "../../../resource/shaders/", "../../../../resource/shaders/",
    };
    for (const char* prefix : prefixes) {
        const std::filesystem::path path = std::filesystem::path{prefix} / filename;
        if (std::filesystem::exists(path)) {
            return path;
        }
    }
    return {};
}

} // namespace

ObjectPickingFeature::ObjectPickingFeature(const Camera& scene_camera)
    : camera(scene_camera)
{
}

bool ObjectPickingFeature::create_pipeline(Context& context) {
    if (context.pipelines.contains(kPipelineName)) {
        return true;
    }
    const auto vert_path = find_shader("object_picking.vert");
    const auto frag_path = find_shader("object_picking.frag");
    if (vert_path.empty() || frag_path.empty()) {
        return false;
    }

    ShaderModule vert;
    ShaderModule frag;
    if (!vert.load(vert_path, vk::ShaderStageFlagBits::eVertex)
        || !frag.load(frag_path, vk::ShaderStageFlagBits::eFragment))
    {
        return false;
    }
    ShaderModulePack pack;
    if (!pack.add_shader_module(vert) || !pack.add_shader_module(frag)) {
        return false;
    }

    PipelineOption option;
    option.setup_input_assembly(vk::PrimitiveTopology::eTriangleList, false);
    option.setup_multisampling(false, vk::SampleCountFlagBits::e1);
    option.setup_rasterizer(false, false, vk::PolygonMode::eFill, 1.0f,
        vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise, false);
    option.setup_depth_stencil(true, true, vk::CompareOp::eLess, false, false);
    // Picked scene meshes currently use interleaved position + normal vertices.
    // The shader consumes only position, but the pipeline stride must match the
    // source mesh so every indexed vertex is read at the correct offset.
    return context.create_pipeline(kPipelineName, pack, option, {VERTEX, NORMAL}, true, false,
        {vk::Format::eR32Uint});
}

void ObjectPickingFeature::on_attach(Context& context, vk::Extent2D) {
    target_index = context.add_render_target(
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
        vk::Format::eR32Uint, 0, 0, vk::ImageLayout::eGeneral);
    ready = target_index != kInvalidTargetIndex && create_pipeline(context);
    update_instance_buffer(context);
}

void ObjectPickingFeature::on_resize(Context&, vk::Extent2D) {
    pick_pending = false;
    last_render_serial = 0;
}

void ObjectPickingFeature::add_object(std::string mesh_name, uint32_t object_id, glm::mat4 model) {
    if (mesh_name.empty() || object_id == 0) {
        return;
    }
    mesh_names.push_back(std::move(mesh_name));
    ObjectPickingInstance instance{};
    instance.model = model;
    instance.object_id = object_id;
    instances.push_back(instance);
    instance_buffer_dirty = true;
}

void ObjectPickingFeature::clear_objects() {
    mesh_names.clear();
    instances.clear();
    instance_buffer_dirty = true;
}

void ObjectPickingFeature::set_pick_callback(std::function<void(uint32_t)> callback) {
    pick_callback = std::move(callback);
}

void ObjectPickingFeature::update_instance_buffer(Context& context) {
    if (!ready || !instance_buffer_dirty) {
        return;
    }
    if (instances.empty()) {
        allocated_instance_count = 0;
        instance_buffer_dirty = false;
        return;
    }
    if (context.resize_pipeline_ssbo(kPipelineName, buf::ObjectPickingInstances, instances.size())
        && context.alloc_pipeline_ssbo(kPipelineName, buf::ObjectPickingInstances))
    {
        allocated_instance_count = instances.size();
        instance_buffer_dirty = false;
    }
}

void ObjectPickingFeature::on_update(Context& context, const Context::Frame& frame) {
    current_serial = frame.serial;
    update_instance_buffer(context);
    if (!ready || context.get_window() == nullptr) {
        return;
    }
    if (pick_pending && last_render_serial >= pending_serial) {
        uint32_t object_id = 0;
        if (context.read_render_target_pixel(target_index, pending_x, pending_y, object_id)) {
            picked_id = object_id;
            if (pick_callback) {
                pick_callback(picked_id);
            }
        }
        pick_pending = false;
    }

    const bool left_down = glfwGetMouseButton(context.get_window(), GLFW_MOUSE_BUTTON_LEFT)
        == GLFW_PRESS;
    if (left_down && !mouse_down) {
        double cursor_x = 0.0;
        double cursor_y = 0.0;
        glfwGetCursorPos(context.get_window(), &cursor_x, &cursor_y);
        int window_width = 0;
        int window_height = 0;
        glfwGetWindowSize(context.get_window(), &window_width, &window_height);
        const auto extent = context.extent();
        if (window_width > 0 && window_height > 0 && extent.width > 0 && extent.height > 0) {
            pending_x = std::min(static_cast<uint32_t>(std::floor(
                cursor_x * static_cast<double>(extent.width) / window_width)), extent.width - 1);
            pending_y = std::min(static_cast<uint32_t>(std::floor(
                cursor_y * static_cast<double>(extent.height) / window_height)), extent.height - 1);
            pending_serial = frame.serial;
            pick_pending = true;
        }
    }
    mouse_down = left_down;
}

void ObjectPickingFeature::on_record(Context& context, vk::raii::CommandBuffer& cmd,
    uint32_t image_index)
{
    if (!ready || !enabled || !pick_pending || instance_buffer_dirty
        || allocated_instance_count != instances.size())
    {
        return;
    }

    ColorTargetRef<uint32_t> color{};
    color.target_index = static_cast<int32_t>(target_index);
    color.clear = {0u, 0u, 0u, 0u};
    PassDescT<uint32_t> pass{};
    pass.colors = {color};
    pass.present = false;
    context.begin_pass(cmd, image_index, pass);
    if (!instances.empty()) {
        context.sync_ubo(kPipelineName, buf::CameraUBO, &camera.ubo_data, image_index);
        context.sync_ssbo(kPipelineName, buf::ObjectPickingInstances, instances.data(), image_index,
            static_cast<uint32_t>(instances.size() * sizeof(ObjectPickingInstance)));
        if (context.bind(cmd, kPipelineName, image_index)) {
            for (uint32_t index = 0; index < static_cast<uint32_t>(instances.size()); ++index) {
                context.draw(cmd, kPipelineName, mesh_names[index], 1, index);
            }
        }
    }
    context.end_pass(cmd, image_index, pass);
    last_render_serial = current_serial;
}

} // namespace vkkk::vp
