#include "vp/grid.hpp"

#include <algorithm>
#include <vector>

#include "concepts/line.h"
#include "vk_ins/shader_module_pack.hpp"

namespace vkkk::vp
{

namespace
{

constexpr const char* kPipelineName = "vp_grid";
constexpr const char* kLinesName = "vp_grid_lines";
constexpr const char* kOriginLinesName = "vp_grid_origin";

bool create_pipeline(Context& context, bool& wide_lines) {
    if (context.pipelines.contains(kPipelineName)) {
        wide_lines = context.wide_lines_enabled;
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
    wide_lines = context.wide_lines_enabled;
    if (wide_lines) {
        option.dynamic_states.push_back(vk::DynamicState::eLineWidth);
        option.dynamic_info.dynamicStateCount =
            static_cast<uint32_t>(option.dynamic_states.size());
        option.dynamic_info.pDynamicStates = option.dynamic_states.data();
    }
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

bool create_origin_lines(Context& context, uint32_t cell_count, float cell_size) {
    if (context.lines.contains(kOriginLinesName) || cell_count == 0 || cell_size <= 0.0f) {
        return context.lines.contains(kOriginLinesName);
    }

    const float extent = static_cast<float>(cell_count) * cell_size * 0.5f;
    const float vertices[] = {
        -extent, 0.0f, 0.0f,
        extent, 0.0f, 0.0f,
        0.0f, 0.0f, -extent,
        0.0f, 0.0f, extent,
    };
    Lines origin({VERTEX});
    origin.load(4, reinterpret_cast<const char*>(vertices), sizeof(vertices));
    return context.load_lines(kOriginLinesName, origin);
}

} // namespace

GridFeature::GridFeature(const Camera& scene_camera, uint32_t cells, float size)
    : camera(scene_camera)
    , cell_count(cells)
    , cell_size(size)
{
}

void GridFeature::on_attach(Context& context, vk::Extent2D) {
    if (!create_grid_lines(context, cell_count, cell_size)
        || !create_origin_lines(context, cell_count, cell_size)
        || !create_pipeline(context, wide_lines))
    {
        return;
    }

    ready = context.resize_pipeline_ssbo(kPipelineName, buf::FixedColorInstanceAttrs, instances.size())
        && context.alloc_pipeline_ssbo(kPipelineName, buf::FixedColorInstanceAttrs);
}

void GridFeature::on_update(Context&, const Context::Frame&) {
    camera_ubo = camera.ubo_data;
    instances[0].model = glm::mat4{1.0f};
    instances[0].color = glm::vec4{0.12f, 0.12f, 0.12f, 1.0f};
    instances[1].model = glm::mat4{1.0f};
    instances[1].color = glm::vec4{0.04f, 0.04f, 0.04f, 1.0f};
}

void GridFeature::on_record(Context& context, vk::raii::CommandBuffer& cmd, uint32_t image_index) {
    if (!ready || !visible) {
        return;
    }

    context.sync_ubo(kPipelineName, buf::CameraUBO, &camera_ubo, image_index);
    context.sync_ssbo(kPipelineName, buf::FixedColorInstanceAttrs, instances.data(), image_index,
        static_cast<uint32_t>(sizeof(instances)));
    if (context.bind(cmd, kPipelineName, image_index)) {
        if (wide_lines) {
            cmd.setLineWidth(1.0f);
        }
        context.draw_lines(cmd, kLinesName, 0, 1, 0);
        if (wide_lines) {
            cmd.setLineWidth(std::clamp(2.0f, context.line_width_range[0],
                context.line_width_range[1]));
        }
        context.draw_lines(cmd, kOriginLinesName, 0, 1, 1);
    }
}

} // namespace vkkk::vp
