#include "renderer/forwardp.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace vkkk
{

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
    }

    return true;
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
void ForwardPRenderer::prepare_light_clusters(const uint32_t swapchain_image_idx) {
    (void)swapchain_image_idx;
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