#include "vp/vertex_picking.hpp"

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

constexpr const char* kPipelineName = "vp_vertex_picking";
constexpr const char* kABufferName = "vp_vertex_picking_abuffer";

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

VertexPickingFeature::VertexPickingFeature(const Camera& scene_camera, uint32_t node_multiplier)
    : camera(scene_camera)
    , nodes_per_pixel(node_multiplier)
{
}

bool VertexPickingFeature::create_pipeline(Context& context) {
    if (context.pipelines.contains(kPipelineName)) {
        return true;
    }
    const auto vert_path = find_shader("vertex_picking.vert");
    const auto frag_path = find_shader("vertex_picking.frag");
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
    option.setup_input_assembly(vk::PrimitiveTopology::ePointList, false);
    option.setup_multisampling(false, vk::SampleCountFlagBits::e1);
    option.setup_rasterizer(false, false, vk::PolygonMode::eFill, 1.0f,
        vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise, false);
    option.setup_depth_stencil(false, false, vk::CompareOp::eAlways, false, false);
    return context.create_pipeline(kPipelineName, pack, option, {VERTEX}, true, true, {},
        static_cast<vk::Format>(context.get_depth_format()));
}

bool VertexPickingFeature::resize_buffers(Context& context, vk::Extent2D extent) {
    return context.resize_abuffer(kABufferName, extent, nodes_per_pixel)
        && context.bind_pipeline_abuffer(kPipelineName, kABufferName, 1, 2);
}

void VertexPickingFeature::on_attach(Context& context, vk::Extent2D extent) {
    ready = nodes_per_pixel != 0 && create_pipeline(context) && resize_buffers(context, extent);
}

void VertexPickingFeature::on_resize(Context& context, vk::Extent2D extent) {
    pick_pending = false;
    last_render_serial = 0;
    ready = ready && resize_buffers(context, extent);
}

void VertexPickingFeature::set_point_list(std::string name, glm::mat4 transform) {
    points_name = std::move(name);
    model = transform;
    picked_ids.clear();
    overflow = false;
}

void VertexPickingFeature::set_point_transform(glm::mat4 transform) {
    model = transform;
}

void VertexPickingFeature::clear_point_list() {
    points_name.clear();
    picked_ids.clear();
    overflow = false;
}

void VertexPickingFeature::set_pick_callback(
    std::function<void(const std::vector<uint32_t>&, bool)> callback)
{
    pick_callback = std::move(callback);
}

void VertexPickingFeature::on_update(Context& context, const Context::Frame& frame) {
    current_serial = frame.serial;
    if (!ready || context.get_window() == nullptr) {
        return;
    }

    if (pick_pending && last_render_serial >= pending_serial) {
        if (context.read_abuffer_pixel(kABufferName, last_image_index, pending_x, pending_y,
                picked_ids, overflow)
            && pick_callback)
        {
            pick_callback(picked_ids, overflow);
        }
        pick_pending = false;
    }

    const bool left_down = glfwGetMouseButton(context.get_window(), GLFW_MOUSE_BUTTON_LEFT)
        == GLFW_PRESS;
    if (left_down && !mouse_down && !points_name.empty()) {
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

void VertexPickingFeature::on_record(Context& context, vk::raii::CommandBuffer& cmd,
    uint32_t image_index)
{
    if (!ready || !enabled || !pick_pending || points_name.empty()
        || !context.clear_abuffer(kABufferName, image_index))
    {
        return;
    }

    PassDesc pass{};
    pass.colors.clear();
    pass.present = false;
    context.begin_pass(cmd, image_index, pass);
    context.sync_ubo(kPipelineName, buf::CameraUBO, &camera.ubo_data, image_index);
    if (context.bind(cmd, kPipelineName, image_index)) {
        VertexPickingParams params{};
        params.model = model;
        params.point_size = std::max(point_size, 1.0f);
        context.push_constants(cmd, kPipelineName, buf::VertexPickingParams, &params,
            static_cast<uint32_t>(sizeof(params)));
        context.draw_points(cmd, points_name);
    }
    context.end_pass(cmd, image_index, pass);
    context.barrier_abuffer_for_host(cmd);
    last_image_index = image_index;
    last_render_serial = current_serial;
}

} // namespace vkkk::vp
