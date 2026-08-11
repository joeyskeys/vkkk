#include "vp/frame_axis.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "concepts/mesh.h"
#include "vk_ins/shader_module_pack.hpp"

namespace vkkk::vp
{

namespace
{

constexpr const char* kPipelineName = "vp_frame_axis";
constexpr const char* kMeshName = "vp_frame_axis_stroke";

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
    option.setup_input_assembly(vk::PrimitiveTopology::eTriangleList, false);
    option.setup_multisampling(false, vk::SampleCountFlagBits::e1);
    option.setup_rasterizer(false, false, vk::PolygonMode::eFill, 1.0f,
        vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise, false);
    option.setup_depth_stencil(false, false, vk::CompareOp::eLess, false, false);
    return context.create_pipeline(kPipelineName, pack, option, {VERTEX});
}

bool create_stroke_mesh(Context& context) {
    if (context.meshes.contains(kMeshName)) {
        return true;
    }

    constexpr float vertices[] = {
        0.0f, -0.018f, 0.0f,
        1.0f, -0.018f, 0.0f,
        1.0f,  0.018f, 0.0f,
        0.0f,  0.018f, 0.0f,
    };
    constexpr uint32_t indices[] = {0, 1, 2, 2, 3, 0};

    Mesh stroke({VERTEX});
    stroke.load(4, reinterpret_cast<const char*>(vertices), sizeof(vertices),
        2, reinterpret_cast<const char*>(indices), sizeof(indices));
    return context.load_mesh(kMeshName, stroke);
}

} // namespace

FrameAxisFeature::FrameAxisFeature(const Camera& camera)
    : camera_(camera)
{
}

void FrameAxisFeature::on_attach(Context& context, vk::Extent2D) {
    if (!create_stroke_mesh(context) || !create_pipeline(context)) {
        return;
    }

    ready = context.resize_pipeline_ssbo(kPipelineName, buf::FixedColorInstanceAttrs, instances_.size())
        && context.alloc_pipeline_ssbo(kPipelineName, buf::FixedColorInstanceAttrs);
}

void FrameAxisFeature::on_update(Context&, const Context::Frame&) {
    const glm::mat4 camera_rotation{glm::mat3(camera_.ubo_data.view)};
    const glm::mat4 anchor = glm::translate(glm::mat4{1.0f}, glm::vec3{-0.82f, -0.78f, 0.0f});
    const glm::mat4 scale = glm::scale(glm::mat4{1.0f}, glm::vec3{0.14f});

    const auto make_model = [&](float angle, const glm::vec3& rotation_axis) {
        return anchor * camera_rotation
            * glm::rotate(glm::mat4{1.0f}, angle, rotation_axis) * scale;
    };

    instances_[0] = FixedColorInstanceAttrs{
        make_model(0.0f, glm::vec3{0.0f, 0.0f, 1.0f}),
        glm::vec4{1.0f, 0.0f, 0.0f, 1.0f},
    };
    instances_[1] = FixedColorInstanceAttrs{
        make_model(glm::half_pi<float>(), glm::vec3{0.0f, 0.0f, 1.0f}),
        glm::vec4{0.0f, 1.0f, 0.0f, 1.0f},
    };
    instances_[2] = FixedColorInstanceAttrs{
        make_model(-glm::half_pi<float>(), glm::vec3{0.0f, 1.0f, 0.0f}),
        glm::vec4{0.0f, 0.0f, 1.0f, 1.0f},
    };

    overlay_camera_.view = glm::mat4{1.0f};
    overlay_camera_.proj = glm::mat4{1.0f};
}

void FrameAxisFeature::on_record(Context& context, vk::raii::CommandBuffer& cmd,
    uint32_t image_index)
{
    if (!ready || !visible) {
        return;
    }

    context.sync_ubo(kPipelineName, buf::CameraUBO, &overlay_camera_, image_index);
    context.sync_ssbo(kPipelineName, buf::FixedColorInstanceAttrs, instances_.data(), image_index,
        static_cast<uint32_t>(sizeof(instances_)));
    if (context.bind(cmd, kPipelineName, image_index)) {
        context.draw(cmd, kPipelineName, kMeshName, static_cast<uint32_t>(instances_.size()));
    }
}

} // namespace vkkk::vp
