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
    camera = nullptr;
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

        sync_uniforms(view.swapchain_image_idx, draw_infos, pipeline);
        if (update_ssbo) {
            sync_ssbos(view.swapchain_image_idx, draw_infos, pipeline);
        }

        const vk::DescriptorSet* desc_set = nullptr;
        if (view.swapchain_image_idx < pipeline.descriptor_sets.size()) {
            desc_set = &*pipeline.descriptor_sets[view.swapchain_image_idx];
        }

        for (const auto& [mesh_name, instance_attrs] : draw_infos) {
            const auto instance_count = static_cast<uint32_t>(instance_attrs.size());
            if (instance_count == 0u) {
                continue;
            }

            ctx->draw_mesh_instanced(
                cmd,
                mesh_name,
                *pipeline.vk_pipeline_layout,
                instance_count,
                desc_set);
        }
    }
}

}