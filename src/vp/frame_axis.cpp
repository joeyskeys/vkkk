#include "vp/frame_axis.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
constexpr std::array<const char*, 3> kLabelTexts = {"X", "Y", "Z"};
const std::array<glm::vec4, 3> kLabelColors = {
    glm::vec4{1.0f, 0.0f, 0.0f, 1.0f},
    glm::vec4{0.0f, 1.0f, 0.0f, 1.0f},
    glm::vec4{0.0f, 0.0f, 1.0f, 1.0f},
};

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
        const auto texture = renderer.render(context, kLabelTexts[index], text_options);
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

FrameAxisFeature::FrameAxisFeature(const Camera& scene_camera, std::filesystem::path font_file)
    : camera(scene_camera)
    , font_path(std::move(font_file))
{
}

void FrameAxisFeature::on_attach(Context& context, vk::Extent2D) {
    if (!create_axis_lines(context) || !create_pipeline(context)) {
        return;
    }

    ready = context.resize_pipeline_ssbo(kPipelineName, buf::FixedColorInstanceAttrs, instances.size())
        && context.alloc_pipeline_ssbo(kPipelineName, buf::FixedColorInstanceAttrs);
    labels_ready = ready && create_axis_labels(context, font_path, label_sizes);
}

void FrameAxisFeature::on_update(Context& context, const Context::Frame&) {
    const glm::mat4 camera_rotation{glm::mat3(camera.ubo_data.view)};
    // Keep the rotated gizmo within Vulkan's [0, 1] clip-space depth range.
    const glm::mat4 anchor = glm::translate(glm::mat4{1.0f}, glm::vec3{-0.82f, -0.78f, 0.5f});
    const glm::mat4 scale = glm::scale(glm::mat4{1.0f}, glm::vec3{0.14f});

    const auto make_model = [&](float angle, const glm::vec3& rotation_axis) {
        return anchor * camera_rotation
            * glm::rotate(glm::mat4{1.0f}, angle, rotation_axis) * scale;
    };

    instances[0].model = make_model(0.0f, glm::vec3{0.0f, 0.0f, 1.0f});
    instances[0].color = kLabelColors[0];
    instances[1].model = make_model(glm::half_pi<float>(), glm::vec3{0.0f, 0.0f, 1.0f});
    instances[1].color = kLabelColors[1];
    instances[2].model = make_model(-glm::half_pi<float>(), glm::vec3{0.0f, 1.0f, 0.0f});
    instances[2].color = kLabelColors[2];

    overlay_camera.view = glm::mat4{1.0f};
    overlay_camera.proj = glm::mat4{1.0f};
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
