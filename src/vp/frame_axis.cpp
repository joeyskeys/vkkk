#include "vp/frame_axis.hpp"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <utility>

#include "concepts/line.h"
#include "font/font.hpp"
#include "vk_ins/shader_module_pack.hpp"

namespace vkkk::vp
{

namespace
{

constexpr const char* kPipelineName = "vp_frame_axis";
constexpr const char* kLinesName = "vp_frame_axis_stroke";
constexpr std::array<const char*, 3> kLabelNames = {
    "vp_frame_axis_label_x",
    "vp_frame_axis_label_y",
    "vp_frame_axis_label_z",
};
const std::array<glm::vec4, 3> kLabelColors = {
    glm::vec4{1.0f, 0.0f, 0.0f, 1.0f},
    glm::vec4{0.0f, 1.0f, 0.0f, 1.0f},
    glm::vec4{0.0f, 0.0f, 1.0f, 1.0f},
};

bool is_orthogonal_frame(const CoordinateSystem& coordinate_system) {
    const auto& axes = coordinate_system.axes;
    constexpr float epsilon = 1.0e-4f;
    if (glm::dot(axes[0], axes[0]) < epsilon || glm::dot(axes[1], axes[1]) < epsilon
        || glm::dot(axes[2], axes[2]) < epsilon)
    {
        return false;
    }
    const glm::vec3 x = glm::normalize(axes[0]);
    const glm::vec3 y = glm::normalize(axes[1]);
    const glm::vec3 z = glm::normalize(axes[2]);
    return std::abs(glm::dot(x, y)) < epsilon
        && std::abs(glm::dot(x, z)) < epsilon
        && std::abs(glm::dot(y, z)) < epsilon;
}

bool create_pipeline(Context& context) {
    if (context.pipelines.contains(kPipelineName)) {
        return true;
    }

    ShaderModule vert_module;
    ShaderModule frag_module;
    if (!vert_module.load(fixed_color_vert, vk::ShaderStageFlagBits::eVertex, "vp_frame_axis_vert")
        || !frag_module.load(fixed_color_frag, vk::ShaderStageFlagBits::eFragment, "vp_frame_axis_frag"))
    {
        return false;
    }

    ShaderModulePack pack;
    if (!pack.add_shader_module(vert_module) || !pack.add_shader_module(frag_module)) {
        return false;
    }

    PipelineOption option;
    option.setup_input_assembly(vk::PrimitiveTopology::eLineList, false);
    option.setup_multisampling(false, vk::SampleCountFlagBits::e1);
    option.setup_rasterizer(false, false, vk::PolygonMode::eFill, 1.0f,
        vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise, false);
    option.setup_depth_stencil(false, false, vk::CompareOp::eLess, false, false);
    return context.create_pipeline(kPipelineName, pack, option, {VERTEX});
}

bool create_axis_lines(Context& context) {
    if (context.lines.contains(kLinesName)) {
        return true;
    }

    constexpr float vertices[] = {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
    };
    constexpr uint32_t indices[] = {
        0, 1,
    };

    Lines axis({VERTEX});
    axis.load(2, reinterpret_cast<const char*>(vertices), sizeof(vertices),
        2, reinterpret_cast<const char*>(indices), sizeof(indices));
    return context.load_lines(kLinesName, axis);
}

bool create_axis_labels(Context& context, const std::filesystem::path& font_path,
    const std::array<std::string, 3>& labels,
    std::array<glm::vec2, 3>& label_sizes)
{
    if (font_path.empty()) {
        return false;
    }

    font::TextRenderer renderer(font_path);
    for (size_t index = 0; index < kLabelNames.size(); ++index) {
        font::TextRenderOptions text_options{};
        text_options.pixel_height = 32;
        text_options.color = kLabelColors[index];
        const auto texture = renderer.render(context, labels[index], text_options);
        if (!texture.valid()) {
            return false;
        }

        constexpr float label_height = 0.035f;
        label_sizes[index] = glm::vec2{
            label_height * static_cast<float>(texture.extent.width) / texture.extent.height,
            label_height,
        };
        BillboardTextOptions billboard_options{};
        billboard_options.size = label_sizes[index];
        billboard_options.depth_test = false;
        if (!context.add_billboard_text(kLabelNames[index],
                BillboardTextSource::render_target(texture.target_index), billboard_options))
        {
            return false;
        }
    }
    return true;
}

} // namespace

FrameAxisFeature::FrameAxisFeature(const Camera& scene_camera, std::filesystem::path font_file,
    CoordinateSystem coordinate_system)
    : camera(scene_camera)
    , font_path(std::move(font_file))
    , coordinate_system(std::move(coordinate_system))
{
}

void FrameAxisFeature::on_attach(Context& context, vk::Extent2D) {
    if (!is_orthogonal_frame(coordinate_system)
        || !create_axis_lines(context) || !create_pipeline(context))
    {
        return;
    }

    ready = context.resize_pipeline_ssbo(kPipelineName, buf::FixedColorInstanceAttrs, instances.size())
        && context.alloc_pipeline_ssbo(kPipelineName, buf::FixedColorInstanceAttrs);
    labels_ready = ready && create_axis_labels(context, font_path, coordinate_system.labels, label_sizes);
}

void FrameAxisFeature::on_update(Context& context, const Context::Frame&) {
    const glm::mat4 camera_rotation{glm::mat3(camera.ubo_data.view)};
    const vk::Extent2D extent = context.extent();
    const float aspect = static_cast<float>(std::max(extent.width, 1u))
        / static_cast<float>(std::max(extent.height, 1u));
    // Keep the rotated gizmo within Vulkan's [0, 1] clip-space depth range.
    const glm::mat4 anchor = glm::translate(
        glm::mat4{1.0f}, glm::vec3{-0.82f * aspect, -0.78f, 0.5f});
    const glm::mat4 scale = glm::scale(glm::mat4{1.0f}, glm::vec3{0.14f});

    const auto make_model = [&](const glm::vec3& axis) {
        const glm::vec3 direction = glm::normalize(axis);
        const glm::vec3 local_x{1.0f, 0.0f, 0.0f};
        const float dot = glm::dot(local_x, direction);
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        if (dot < -0.9999f) {
            rotation = glm::angleAxis(glm::pi<float>(), glm::vec3{0.0f, 1.0f, 0.0f});
        }
        else if (dot < 0.9999f) {
            rotation = glm::angleAxis(
                std::acos(std::clamp(dot, -1.0f, 1.0f)),
                glm::normalize(glm::cross(local_x, direction)));
        }
        return anchor * camera_rotation
            * glm::mat4_cast(rotation) * scale;
    };

    instances[0].model = make_model(coordinate_system.axes[0]);
    instances[0].color = kLabelColors[0];
    instances[1].model = make_model(coordinate_system.axes[1]);
    instances[1].color = kLabelColors[1];
    instances[2].model = make_model(coordinate_system.axes[2]);
    instances[2].color = kLabelColors[2];

    overlay_camera.view = glm::mat4{1.0f};
    overlay_camera.proj = glm::mat4{1.0f};
    // NDC spans the viewport's width and height independently. Compensate X
    // so a unit overlay-space vector has the same pixel scale in X and Y.
    overlay_camera.proj[0][0] = 1.0f / aspect;
    overlay_camera.proj[1][1] = -1.0f;

    if (labels_ready) {
        for (size_t index = 0; index < kLabelNames.size(); ++index) {
            const glm::vec3 position{instances[index].model
                * glm::vec4{1.18f, 0.0f, 0.0f, 1.0f}};
            context.set_billboard_text_transform(kLabelNames[index], position, label_sizes[index]);
        }
    }
}

void FrameAxisFeature::on_record(Context& context, vk::raii::CommandBuffer& cmd,
    uint32_t image_index)
{
    if (!ready || !visible) {
        return;
    }

    context.sync_ubo(kPipelineName, buf::CameraUBO, &overlay_camera, image_index);
    context.sync_ssbo(kPipelineName, buf::FixedColorInstanceAttrs, instances.data(), image_index,
        static_cast<uint32_t>(sizeof(instances)));
    if (context.bind(cmd, kPipelineName, image_index)) {
        context.draw_lines(cmd, kLinesName, 0, static_cast<uint32_t>(instances.size()));
    }
    if (labels_ready) {
        for (const auto* label_name : kLabelNames) {
            context.draw_billboard_text(cmd, label_name, overlay_camera, image_index);
        }
    }
}

} // namespace vkkk::vp
