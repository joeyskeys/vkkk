#include <algorithm>
#include <cmath>
#include <vector>

#include "built_in_shader/billboard_text.h"
#include "concepts/mesh.h"
#include "vk_ins/context.hpp"

namespace vkkk
{

namespace
{

constexpr uint32_t kTextSamplerBinding = 2;

bool is_valid_source(const Context& context, const BillboardTextSource& source) {
    if (source.type == BillboardTextSourceType::Texture) {
        const auto texture = context.textures.find(source.texture_name);
        return texture != context.textures.end() && texture->second.vecsize == 1;
    }

    return source.target_index < context.targets.size()
        && (context.targets[source.target_index].usage & vk::ImageUsageFlagBits::eSampled)
            != vk::ImageUsageFlags{};
}

bool is_valid_options(const BillboardTextOptions& options) {
    return std::isfinite(options.position.x) && std::isfinite(options.position.y)
        && std::isfinite(options.position.z) && std::isfinite(options.size.x)
        && std::isfinite(options.size.y) && options.size.x > 0.0f && options.size.y > 0.0f;
}

bool create_billboard_pipeline(Context& context, const std::string& pipeline_name, bool depth_test) {
    ShaderModule vert_module;
    ShaderModule frag_module;
    if (!vert_module.load(billboard_text_vert, vk::ShaderStageFlagBits::eVertex, "billboard_text_vert")
        || !frag_module.load(billboard_text_frag, vk::ShaderStageFlagBits::eFragment, "billboard_text_frag"))
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
    option.setup_depth_stencil(depth_test, false, vk::CompareOp::eLessOrEqual, false, false);
    option.blend_attachment_info.blendEnable = vk::True;
    option.blend_attachment_info.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    option.blend_attachment_info.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    option.blend_attachment_info.colorBlendOp = vk::BlendOp::eAdd;
    option.blend_attachment_info.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    option.blend_attachment_info.dstAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    option.blend_attachment_info.alphaBlendOp = vk::BlendOp::eAdd;
    return context.create_pipeline(pipeline_name, pack, option, {VERTEX, UV});
}

} // namespace

bool Context::add_billboard_text(const std::string& name, const BillboardTextSource& source,
    const BillboardTextOptions& options)
{
    const std::array<BillboardTextVertex, 4> vertices = {{
        {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, 0.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, 0.0f}, {0.0f, 1.0f}},
    }};
    return add_billboard_text_quad(name, vertices, source, options);
}

bool Context::add_billboard_text_quad(const std::string& name,
    const std::array<BillboardTextVertex, 4>& vertices, const BillboardTextSource& source,
    const BillboardTextOptions& options)
{
    if (name.empty() || billboard_texts.contains(name) || !is_valid_source(*this, source)
        || !is_valid_options(options))
    {
        return false;
    }

    const std::string pipeline_name = "__billboard_text_pipeline_" + name;
    const std::string mesh_name = "__billboard_text_mesh_" + name;
    if (pipelines.contains(pipeline_name) || meshes.contains(mesh_name)) {
        return false;
    }

    std::vector<float> vertex_data;
    vertex_data.reserve(vertices.size() * 5);
    for (const auto& vertex : vertices) {
        if (!std::isfinite(vertex.position.x) || !std::isfinite(vertex.position.y)
            || !std::isfinite(vertex.position.z) || !std::isfinite(vertex.uv.x)
            || !std::isfinite(vertex.uv.y))
        {
            return false;
        }
        vertex_data.insert(vertex_data.end(), {
            vertex.position.x, vertex.position.y, vertex.position.z, vertex.uv.x, vertex.uv.y});
    }

    if (!create_billboard_pipeline(*this, pipeline_name, options.depth_test)) {
        return false;
    }
    const auto discard_pipeline = [&] {
        pipelines.erase(pipeline_name);
        std::erase_if(sampled_attachment_binds,
            [&pipeline_name](const SampledAttachmentBind& bind) {
                return bind.pipeline_name == pipeline_name;
            });
    };
    const bool source_bound = source.type == BillboardTextSourceType::Texture
        ? bind_pipeline_texture(pipeline_name, kTextSamplerBinding, source.texture_name)
        : bind_pipeline_render_target(pipeline_name, kTextSamplerBinding, source.target_index);
    if (!source_bound) {
        discard_pipeline();
        return false;
    }

    constexpr uint32_t indices[] = {0, 1, 2, 2, 3, 0};
    Mesh mesh({VERTEX, UV});
    mesh.load(static_cast<uint32_t>(vertices.size()),
        reinterpret_cast<const char*>(vertex_data.data()),
        static_cast<uint32_t>(vertex_data.size() * sizeof(float)), 2,
        reinterpret_cast<const char*>(indices), sizeof(indices));
    if (!load_mesh(mesh_name, mesh)) {
        discard_pipeline();
        return false;
    }

    billboard_texts.emplace(name, BillboardText{pipeline_name, mesh_name, options});
    return true;
}

bool Context::set_billboard_text_transform(const std::string& name, const glm::vec3& position,
    const glm::vec2& size)
{
    const auto found = billboard_texts.find(name);
    if (found == billboard_texts.end()) {
        return false;
    }
    const BillboardTextOptions options{position, size, found->second.options.depth_test};
    if (!is_valid_options(options)) {
        return false;
    }

    found->second.options.position = position;
    found->second.options.size = size;
    return true;
}

bool Context::draw_billboard_text(vk::CommandBuffer cmd, const std::string& name,
    const CameraUBO& camera, uint32_t frame_idx)
{
    const auto found = billboard_texts.find(name);
    if (found == billboard_texts.end()) {
        return false;
    }

    const auto& text = found->second;
    const BillboardTextData data{
        glm::vec4{text.options.position, 1.0f},
        glm::vec4{text.options.size, 1.0f, 0.0f},
    };
    return sync_ubo(text.pipeline_name, buf::CameraUBO, &camera, frame_idx)
        && sync_ubo(text.pipeline_name, buf::BillboardTextData, &data, frame_idx)
        && bind(cmd, text.pipeline_name, frame_idx)
        && draw(cmd, text.pipeline_name, text.mesh_name, 1);
}

} // namespace vkkk
