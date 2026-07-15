#include "renderer/forwardp.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include "asset_mgr/light_mgr.h"
#include "asset_mgr/scene.h"
#include "built_in_shader/light_clusterize.h"
#include "concepts/camera.h"

namespace vkkk
{

namespace
{

constexpr char light_cluster_pipeline_name[] = "forwardp_light_cluster";
constexpr uint32_t cluster_tile_size = 16;
constexpr uint32_t cluster_depth_slices = 24;

} // namespace

// public funcs
bool ForwardPRenderer::initialize(Context* context) {
    ctx = context;
    if (!ctx) {
        return false;
    }

    if (ctx->get_window()) {
        int w = 0;
        int h = 0;
        glfwGetFramebufferSize(ctx->get_window(), &w, &h);
        width = static_cast<uint32_t>(std::max(w, 1));
        height = static_cast<uint32_t>(std::max(h, 1));
    }

    ComputeShader cluster_shader;
    if (!cluster_shader.load(light_clusterize_comp, light_cluster_pipeline_name)) {
        return false;
    }
    return ctx->create_compute_pipeline(light_cluster_pipeline_name, cluster_shader);
}

void ForwardPRenderer::shutdown() {
    opaque_batches.clear();
    transparent_batches.clear();
    ctx = nullptr;
    scene = nullptr;
}

void ForwardPRenderer::on_resize(uint32_t width, uint32_t height) {
    this->width = width;
    this->height = height;
}

void ForwardPRenderer::record_commands(vk::CommandBuffer cmd, const uint32_t swapchain_image_idx) {
    if (!ctx) {
        return;
    }

    pass_opaque(cmd, swapchain_image_idx);
    pass_transparent(cmd, swapchain_image_idx);
}

void ForwardPRenderer::allocate_ssbo() {
    size_t total_opaque_size = 0;
    size_t total_transparent_size = 0;

    auto allocate_ssbo_for_batch = [](const IntermediateSSBODataMap& intermediate_ssbo_data_map, Batches& batches) {
        for (const auto& [pipeline_name, intermediate_ssbo_data] : intermediate_ssbo_data_map) {
            size_t instance_offset = 0;
            size_t byte_offset = 0;
            batches[pipeline_name].buffer = std::make_unique<char[]>(intermediate_ssbo_data.total_size);
            std::vector<BatchInfo> batch_infos(intermediate_ssbo_data.data_map.size());
            for (int i = 0; const auto& [mesh_name, intermediate_ssbo_buffer] : intermediate_ssbo_data.data_map) {
                memcpy(batches[pipeline_name].buffer.get() + byte_offset, intermediate_ssbo_buffer.data.data(), intermediate_ssbo_buffer.data.size());
                batch_infos[i] = BatchInfo{
                    .mesh_name = mesh_name,
                    .batch_total_size = intermediate_ssbo_buffer.data.size(),
                    .instance_count = intermediate_ssbo_buffer.instance_count,
                    .instance_offset = instance_offset,
                };
                instance_offset += intermediate_ssbo_buffer.instance_count;
                byte_offset += intermediate_ssbo_buffer.data.size();
                i++;
            }
            batches[pipeline_name].batch_infos = std::move(batch_infos);
        }
    };
    allocate_ssbo_for_batch(opaque_intermediate_ssbo_data_map, opaque_batches);
    allocate_ssbo_for_batch(transparent_intermediate_ssbo_data_map, transparent_batches);
}

// private funcs
void ForwardPRenderer::prepare_light_clusters(vk::CommandBuffer cmd, const uint32_t swapchain_image_idx) {
    if (!ctx || !scene || !scene->camera || !scene->light_mgr) {
        return;
    }

    const PipelineLightStorage* light_storage = nullptr;
    for (const auto& [pipeline_name, _] : opaque_batches) {
        light_storage = scene->light_mgr->pipeline_storage(pipeline_name);
        if (light_storage != nullptr) {
            break;
        }
    }
    if (light_storage == nullptr) {
        for (const auto& [pipeline_name, _] : transparent_batches) {
            light_storage = scene->light_mgr->pipeline_storage(pipeline_name);
            if (light_storage != nullptr) {
                break;
            }
        }
    }
    if (light_storage == nullptr) {
        return;
    }

    const uint32_t cluster_x = std::max((width + cluster_tile_size - 1) / cluster_tile_size, 1u);
    const uint32_t cluster_y = std::max((height + cluster_tile_size - 1) / cluster_tile_size, 1u);
    const uint32_t cluster_count = cluster_x * cluster_y * cluster_depth_slices;
    const uint32_t light_index_capacity = cluster_count * max_lights_per_cluster;

    const std::string params_name = std::string(light_cluster_pipeline_name) + ":LightClusterParams";

    const size_t point_light_capacity = std::max(light_storage->pt_lights.size(), size_t{1});
    if (!ctx->resize_compute_ssbo(light_cluster_pipeline_name, SSBOType_PointLights, point_light_capacity)
        || !ctx->resize_compute_ssbo(light_cluster_pipeline_name, SSBOType_ClusterGrid, cluster_count)
        || !ctx->resize_compute_ssbo(light_cluster_pipeline_name, SSBOType_ClusterLightIndices, light_index_capacity)
        || !ctx->alloc_compute_ssbo(light_cluster_pipeline_name, SSBOType_PointLights)
        || !ctx->alloc_compute_ssbo(light_cluster_pipeline_name, SSBOType_ClusterGrid)
        || !ctx->alloc_compute_ssbo(light_cluster_pipeline_name, SSBOType_ClusterLightIndices))
    {
        return;
    }

    const auto bind_cluster_buffers = [&](const Batches& batches) {
        for (const auto& [pipeline_name, _] : batches) {
            const auto pipeline_it = ctx->pipelines.find(pipeline_name);
            if (pipeline_it == ctx->pipelines.end()) {
                continue;
            }
            const auto& ssbos = pipeline_it->second.ssbos;
            if (ssbos.contains(SSBOType_PointLights)) {
                ctx->bind_pipeline_ssbo_from_compute(
                    pipeline_name, SSBOType_PointLights,
                    light_cluster_pipeline_name, SSBOType_PointLights);
            }
            if (ssbos.contains(SSBOType_ClusterGrid)) {
                ctx->bind_pipeline_ssbo_from_compute(
                    pipeline_name, SSBOType_ClusterGrid,
                    light_cluster_pipeline_name, SSBOType_ClusterGrid);
            }
            if (ssbos.contains(SSBOType_ClusterLightIndices)) {
                ctx->bind_pipeline_ssbo_from_compute(
                    pipeline_name, SSBOType_ClusterLightIndices,
                    light_cluster_pipeline_name, SSBOType_ClusterLightIndices);
            }
        }
    };
    bind_cluster_buffers(opaque_batches);
    bind_cluster_buffers(transparent_batches);

    const PointLightUBO empty_light{};
    const void* light_data = light_storage->pt_lights.empty()
        ? static_cast<const void*>(&empty_light)
        : static_cast<const void*>(light_storage->pt_lights.data());
    const uint32_t light_bytes = static_cast<uint32_t>(
        light_storage->pt_lights.size() * sizeof(PointLightUBO));
    if (!ctx->sync_compute_ssbo(
            light_cluster_pipeline_name,
            SSBOType_PointLights,
            light_data,
            swapchain_image_idx,
            light_bytes))
    {
        return;
    }

    LightClusterParamsUBO params{};
    params.view = scene->camera->ubo_data.view;
    params.proj = scene->camera->ubo_data.proj;
    params.cluster_dims_and_light_count = glm::uvec4(
        cluster_x, cluster_y, cluster_depth_slices,
        static_cast<uint32_t>(light_storage->pt_lights.size()));
    params.config = glm::uvec4(max_lights_per_cluster, width, height, 0u);
    params.depth_range = glm::vec4(scene->camera->near, scene->camera->far, 0.0f, 0.0f);

    try {
        auto& params_ubo = ctx->require_ubo(params_name);
        if (swapchain_image_idx >= params_ubo.memos.size()) {
            return;
        }
        ctx->sync_uniform(params_ubo.memos[swapchain_image_idx], &params, sizeof(params));
    }
    catch (const std::runtime_error&) {
        return;
    }

    const auto sync_graphics_cluster_params = [&](const Batches& batches) {
        for (const auto& [pipeline_name, _] : batches) {
            const auto pipeline_it = ctx->pipelines.find(pipeline_name);
            if (pipeline_it == ctx->pipelines.end()) {
                continue;
            }
            const auto ubo_it = pipeline_it->second.ubos.find(UBOType_LightClusterParams);
            if (ubo_it != pipeline_it->second.ubos.end()
                && swapchain_image_idx < ubo_it->second.memos.size())
            {
                ctx->sync_uniform(
                    ubo_it->second.memos[swapchain_image_idx],
                    &params,
                    static_cast<uint32_t>(sizeof(params)));
            }
        }
    };
    sync_graphics_cluster_params(opaque_batches);
    sync_graphics_cluster_params(transparent_batches);

    ctx->record_compute(
        cmd,
        light_cluster_pipeline_name,
        cluster_x,
        cluster_y,
        cluster_depth_slices,
        swapchain_image_idx);
}

void ForwardPRenderer::pass_shadow(vk::CommandBuffer cmd, const uint32_t swapchain_image_idx) {
    (void)cmd;
    (void)swapchain_image_idx;
}

void ForwardPRenderer::draw_batch(vk::CommandBuffer cmd, const uint32_t swapchain_image_idx, const Batches& batches) {
    if (!ctx) {
        return;
    }

    for (const auto& [pipeline_name, batch] : batches) {
        const auto pipeline_it = ctx->pipelines.find(pipeline_name);
        if (pipeline_it == ctx->pipelines.end()) {
            continue;
        }

        const auto& pipeline = pipeline_it->second;
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline.vk_pipeline);

        sync_uniforms(swapchain_image_idx, scene, pipeline_name, pipeline);

        const vk::DescriptorSet* desc_set = nullptr;
        if (swapchain_image_idx < pipeline.descriptor_sets.size()) {
            desc_set = &*pipeline.descriptor_sets[swapchain_image_idx];
        }

        const auto ssbo_it = pipeline.ssbos.find(SSBOType_InstanceAttrs);
        const size_t elem_size = ssbo_it != pipeline.ssbos.end() ? ssbo_it->second.size : 0;
        size_t upload_bytes = 0;
        for (const auto& batch_info : batch.batch_infos) {
            upload_bytes = std::max(upload_bytes,
                (batch_info.instance_offset + batch_info.instance_count) * elem_size);
        }
        sync_ssbo(batch.buffer.get(), pipeline, swapchain_image_idx, static_cast<uint32_t>(upload_bytes));
        for (const auto& batch_info : batch.batch_infos) {
            ctx->draw_mesh_instanced(
                cmd,
                batch_info.mesh_name,
                *pipeline.vk_pipeline_layout,
                batch_info.instance_count,
                batch_info.instance_offset,
                desc_set
            );
        }
    }
}

}