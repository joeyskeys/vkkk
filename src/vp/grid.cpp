#include "vp/grid.hpp"

#include <vector>

#include "concepts/line.h"
#include "vk_ins/shader_module_pack.hpp"

namespace vkkk::vp
{

namespace
{

constexpr const char* kPipelineName = "vp_grid";
constexpr const char* kLinesName = "vp_grid_lines";

bool create_pipeline(Context& context) {
    if (context.pipelines.contains(kPipelineName)) {
        return true;
    }

    ShaderModule vert_module;
    ShaderModule frag_module;
    if (!vert_module.load(fixed_color_vert, vk::ShaderStageFlagBits::eVertex, "vp_grid_vert")
        || !frag_module.load(fixed_color_frag, vk::ShaderStageFlagBits::eFragment, "vp_grid_frag"))
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
    option.setup_depth_stencil(true, false, vk::CompareOp::eLessOrEqual, false, false);
    return context.create_pipeline(kPipelineName, pack, option, {VERTEX});
}

bool create_grid_lines(Context& context, uint32_t cell_count, float cell_size) {
    if (context.lines.contains(kLinesName) || cell_count == 0 || cell_size <= 0.0f) {
        return context.lines.contains(kLinesName);
    }

    const float extent = static_cast<float>(cell_count) * cell_size * 0.5f;
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    const uint32_t vertex_width = cell_count + 1;
    vertices.reserve(vertex_width * vertex_width * 3);
    indices.reserve(cell_count * vertex_width * 4);

    const auto append_vertex = [&vertices](float x, float y, float z) {
        vertices.insert(vertices.end(), {x, y, z});
    };

    for (uint32_t z = 0; z <= cell_count; ++z) {
        for (uint32_t x = 0; x <= cell_count; ++x) {
            append_vertex(
                -extent + static_cast<float>(x) * cell_size,
                0.0f,
                -extent + static_cast<float>(z) * cell_size);
        }
    }

    const auto vertex_index = [vertex_width](uint32_t x, uint32_t z) {
        return z * vertex_width + x;
    };
    for (uint32_t z = 0; z <= cell_count; ++z) {
        for (uint32_t x = 0; x < cell_count; ++x) {
            indices.insert(indices.end(), {vertex_index(x, z), vertex_index(x + 1, z)});
        }
    }
    for (uint32_t x = 0; x <= cell_count; ++x) {
        for (uint32_t z = 0; z < cell_count; ++z) {
            indices.insert(indices.end(), {vertex_index(x, z), vertex_index(x, z + 1)});
        }
    }

    Lines grid({VERTEX});
    grid.load(static_cast<uint32_t>(vertices.size() / 3),
        reinterpret_cast<const char*>(vertices.data()),
        static_cast<uint32_t>(vertices.size() * sizeof(float)),
        static_cast<uint32_t>(indices.size()),
        reinterpret_cast<const char*>(indices.data()),
        static_cast<uint32_t>(indices.size() * sizeof(uint32_t)));
    return context.load_lines(kLinesName, grid);
}

} // namespace

GridFeature::GridFeature(const Camera& scene_camera, uint32_t cells, float size)
    : camera(scene_camera)
    , cell_count(cells)
    , cell_size(size)
{
}

void GridFeature::on_attach(Context& context, vk::Extent2D) {
    if (!create_grid_lines(context, cell_count, cell_size) || !create_pipeline(context)) {
        return;
    }

    ready = context.resize_pipeline_ssbo(kPipelineName, buf::FixedColorInstanceAttrs, instances.size())
        && context.alloc_pipeline_ssbo(kPipelineName, buf::FixedColorInstanceAttrs);
}

void GridFeature::on_update(Context&, const Context::Frame&) {
    camera_ubo = camera.ubo_data;
    instances[0].model = glm::mat4{1.0f};
    instances[0].color = glm::vec4{0.12f, 0.12f, 0.12f, 1.0f};
}

void GridFeature::on_record(Context& context, vk::raii::CommandBuffer& cmd, uint32_t image_index) {
    if (!ready || !visible) {
        return;
    }

    context.sync_ubo(kPipelineName, buf::CameraUBO, &camera_ubo, image_index);
    context.sync_ssbo(kPipelineName, buf::FixedColorInstanceAttrs, instances.data(), image_index,
        static_cast<uint32_t>(sizeof(instances)));
    if (context.bind(cmd, kPipelineName, image_index)) {
        context.draw_lines(cmd, kLinesName, 0, static_cast<uint32_t>(instances.size()));
    }
}

} // namespace vkkk::vp
