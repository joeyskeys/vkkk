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
    opaque_batch.clear();
    transparent_batch.clear();
    ctx = nullptr;
    scene = nullptr;
}

void ForwardPRenderer::on_resize(uint32_t width, uint32_t height) {
    this->width = width;
    this->height = height;
}

void ForwardPRenderer::record_commands(vk::CommandBuffer cmd, const RenderView& view) {
    if (!ctx) {
        return;
    }

    pass_opaque(cmd, view);
    pass_transparent(cmd, view);
}

void ForwardPRenderer::allocate_ssbo() {
    size_t total_opaque_size = 0;
    size_t total_transparent_size = 0;

    auto allocate_ssbo_for_batch = [](const IntermediateSSBOData& intermediate_ssbo_data, Batch& batch) {
        size_t total_size = 0;
        for (const auto& [pipeline_name, buf_pair] : intermediate_ssbo_data) {
            batch[pipeline_name].first = std::make_unique<char[]>(buf_pair.first);
            std::vector<std::tuple<std::string, size_t, size_t>> draw_infos(buf_pair.second.size());
            for (int i = 0; const auto& [mesh_name, buf_info] : buf_pair.second) {
                memcpy(batch[pipeline_name].first.get() + total_size, buf_info.second.data(), buf_info.second.size());
                draw_infos[i] = std::make_tuple(mesh_name, buf_info.first, total_size);
                total_size += buf_info.second.size();
                i++;
            }
            batch[pipeline_name].second = draw_infos;
        }
    };
    allocate_ssbo_for_batch(opaque_intermediate_ssbo_data, opaque_batch);
    allocate_ssbo_for_batch(transparent_intermediate_ssbo_data, transparent_batch);
}

// private funcs
void ForwardPRenderer::prepare_light_clusters(const RenderView& view) {
    (void)view;
}

void ForwardPRenderer::pass_shadow(vk::CommandBuffer cmd, const RenderView& view) {
    (void)cmd;
    (void)view;
}

void ForwardPRenderer::draw_batch(vk::CommandBuffer cmd, const RenderView& view, const Batch& batch) {
    if (!ctx) {
        return;
    }

    for (const auto& [pipeline_name, draw_infos] : batch) {
        const auto pipeline_it = ctx->pipelines.find(pipeline_name);
        if (pipeline_it == ctx->pipelines.end()) {
            continue;
        }

        const auto& pipeline = pipeline_it->second;
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline.vk_pipeline);

        sync_uniforms(view.swapchain_image_idx, scene, pipeline_name, pipeline);

        const vk::DescriptorSet* desc_set = nullptr;
        if (view.swapchain_image_idx < pipeline.descriptor_sets.size()) {
            desc_set = &*pipeline.descriptor_sets[view.swapchain_image_idx];
        }

		sync_ssbo(draw_infos.first.get(), pipeline);
        for (const auto& [mesh_name, instance_count, offset] : draw_infos.second) {
            ctx->draw_mesh_instanced(
                cmd,
                mesh_name,
                *pipeline.vk_pipeline_layout,
                instance_count,
                offset,
                desc_set
            );
        }
    }
}

}