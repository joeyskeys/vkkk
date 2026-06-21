#include "renderer/forward_plus.h"

#include <algorithm>
#include <array>
#include <utility>

#include <glm/gtc/matrix_transform.hpp>

#include "asset_mgr/light_mgr.h"
#include "asset_mgr/scene.h"
#include "concepts/camera.h"
#include "vk_ins/compute_shader.hpp"

namespace vkkk
{

bool ForwardPlusRenderer::initialize(Context* context) {
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

    return true;
}

void ForwardPlusRenderer::shutdown() {
    ctx = nullptr;
    scene = nullptr;
    camera = nullptr;
}

void ForwardPlusRenderer::on_resize(uint32_t width, uint32_t height) {
    this->width = width;
    this->height = height;
}

void ForwardPlusRenderer::update_camera_aspect() {
    if (!camera || height == 0) {
        return;
    }
    camera->ratio = width / static_cast<float>(height);
}

void ForwardPlusRenderer::update_lights_from_scene() {
    light_ubo_.lightPos = glm::vec4(0.0f, 5.0f, 5.0f, 1.0f);
    light_ubo_.lightColor = glm::vec4(1.0f);

    if (scene && scene->light_mgr) {
        const auto& pt_lights = scene->light_mgr->point_lights();
        if (!pt_lights.empty()) {
            light_ubo_.lightPos = pt_lights[0].pos;
            light_ubo_.lightColor = pt_lights[0].color;
        }
    }

    light_ubo_.viewPos = camera
        ? glm::vec4(camera->pos, 1.0f)
        : glm::vec4(0.0f, 0.0f, 5.0f, 1.0f);
}

void ForwardPlusRenderer::update_shadow_from_scene() {
    const glm::vec3 light_pos = glm::vec3(light_ubo_.lightPos);
    const glm::vec3 light_target(0.0f, 0.0f, 0.0f);
    const glm::vec3 light_up(0.0f, 1.0f, 0.0f);
    shadow_light_view_ = glm::lookAt(light_pos, light_target, light_up);
    shadow_light_proj_ = glm::ortho(-25.0f, 25.0f, -25.0f, 25.0f, 0.1f, 100.0f);
    shadow_light_proj_[1][1] *= -1.0f;
    shadow_params_ubo_.lightSpace = shadow_light_proj_ * shadow_light_view_;
}

void ForwardPlusRenderer::update_global_uniforms(uint32_t swapchain_idx) {
    if (!ctx || !camera) {
        return;
    }

    PhongTransformUBO camera_transform{};
    camera_transform.model = glm::mat4(1.0f);
    camera_transform.view = camera->get_view_mat();
    camera_transform.proj = camera->get_proj_mat();

    auto sync_global_ubo = [&](const char* pipeline_name, const char* suffix,
                               const void* data, size_t size) {
        const auto ubo_name = std::string(pipeline_name) + suffix;
        auto found = ctx->ubos.find(ubo_name);
        if (found == ctx->ubos.end() || swapchain_idx >= found->second.memos.size()) {
            return;
        }
        ctx->sync_uniform(found->second.memos[swapchain_idx], data, static_cast<uint32_t>(size));
    };

    for (const auto* pipeline_name : {kOpaquePipeline, kTransparentPipeline}) {
        sync_global_ubo(pipeline_name, ":UniformBufferObject", &camera_transform, sizeof(camera_transform));
        sync_global_ubo(pipeline_name, ":PhongLight", &light_ubo_, sizeof(light_ubo_));
        sync_global_ubo(pipeline_name, ":ShadowParams", &shadow_params_ubo_, sizeof(shadow_params_ubo_));
    }
}

void ForwardPlusRenderer::sync_draw_item_uniforms(uint32_t swapchain_idx,
    const ForwardPlusDrawItem& item, const std::string& pipeline_name)
{
    if (!ctx || !camera || pipeline_name.empty()) {
        return;
    }

    PhongTransformUBO transform_ubo{};
    transform_ubo.model = item.model;
    transform_ubo.view = camera->get_view_mat();
    transform_ubo.proj = camera->get_proj_mat();

    auto sync_if_present = [&](const std::string& ubo_suffix, const void* data, size_t size) {
        const auto ubo_name = pipeline_name + ubo_suffix;
        auto found = ctx->ubos.find(ubo_name);
        if (found == ctx->ubos.end() || swapchain_idx >= found->second.memos.size()) {
            return;
        }
        ctx->sync_uniform(found->second.memos[swapchain_idx], data, static_cast<uint32_t>(size));
    };

    sync_if_present(":ubo", &transform_ubo, sizeof(transform_ubo));
    sync_if_present(":UniformBufferObject", &transform_ubo, sizeof(transform_ubo));
    sync_if_present(":material", &item.material, sizeof(item.material));
    sync_if_present(":PhongMaterial", &item.material, sizeof(item.material));
    sync_if_present(":light", &light_ubo_, sizeof(light_ubo_));
    sync_if_present(":PhongLight", &light_ubo_, sizeof(light_ubo_));
    sync_if_present(":ShadowParams", &shadow_params_ubo_, sizeof(shadow_params_ubo_));
}

void ForwardPlusRenderer::sync_shadow_uniforms(uint32_t swapchain_idx, const ForwardPlusDrawItem& item) {
    if (!ctx) {
        return;
    }

    shadow_transform_ubo_.model = item.model;
    shadow_transform_ubo_.lightView = shadow_light_view_;
    shadow_transform_ubo_.lightProj = shadow_light_proj_;

    const auto ubo_name = std::string(kShadowPipeline) + ":ShadowTransform";
    auto found = ctx->ubos.find(ubo_name);
    if (found == ctx->ubos.end() || swapchain_idx >= found->second.memos.size()) {
        return;
    }
    ctx->sync_uniform(found->second.memos[swapchain_idx], &shadow_transform_ubo_, sizeof(shadow_transform_ubo_));
}

void ForwardPlusRenderer::prepare_light_clusters(const RenderView& view) {
    if (!ctx) {
        return;
    }

    if (ctx->compute_pipelines.find(kLightClusterComputePipeline) == ctx->compute_pipelines.end()) {
        ComputeShader cluster_shader;
        if (!cluster_shader.load(kLightClusterComputeShader, "forward_plus_light_clusters.comp")) {
            return;
        }
        if (!ctx->create_compute_pipeline(kLightClusterComputePipeline, cluster_shader)) {
            return;
        }
    }

    const uint32_t dispatch_x = std::max(1u, (width + 15u) / 16u);
    const uint32_t dispatch_y = std::max(1u, (height + 15u) / 16u);
    ctx->dispatch_compute(kLightClusterComputePipeline, dispatch_x, dispatch_y, 1u, view.swapchain_image_idx);
}

void ForwardPlusRenderer::pass_shadow(vk::CommandBuffer cmd, const RenderView& view) {
    if (!ctx) {
        return;
    }
    const auto pipeline_found = ctx->pipelines.find(kShadowPipeline);
    if (pipeline_found == ctx->pipelines.end()) {
        return;
    }

    const auto& pipeline = pipeline_found->second;
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline.vk_pipeline);

    for (const auto& item : draw_items_) {
        if (item.transparent) {
            continue;
        }

        const auto mesh_found = ctx->meshes.find(item.mesh_name);
        if (mesh_found == ctx->meshes.end()) {
            continue;
        }

        sync_shadow_uniforms(view.swapchain_image_idx, item);
        const vk::DescriptorSet* desc_set = nullptr;
        if (view.swapchain_image_idx < pipeline.descriptor_sets.size()) {
            desc_set = &*pipeline.descriptor_sets[view.swapchain_image_idx];
        }
        mesh_found->second.emit_draw_cmd(cmd, *pipeline.vk_pipeline_layout, desc_set);
    }
}

void ForwardPlusRenderer::update(const RenderView& view) {
    update_camera_aspect();
    update_lights_from_scene();
    update_shadow_from_scene();
    prepare_light_clusters(view);
    update_global_uniforms(view.swapchain_image_idx);
    (void)view.delta_seconds;
}

void ForwardPlusRenderer::record_commands(vk::CommandBuffer cmd, const RenderView& view) {
    if (!ctx) {
        return;
    }

    pass_shadow(cmd, view);
    pass_opaque(cmd, view);
    pass_transparent(cmd, view);
}

} // namespace vkkk
