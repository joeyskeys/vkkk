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

namespace
{

using built_in_shader::PhongTransformUBO;

constexpr std::array<const char*, 4> kShaderSearchRoots = {
    "resource/shaders/forward/",
    "../resource/shaders/forward/",
    "../../resource/shaders/forward/",
    "../../../resource/shaders/forward/",
};

constexpr const char* kLightClusterComputePipeline = "forward_plus_light_clusters";
constexpr const char* kLightClusterComputeShader = R"(
#version 450
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
void main() {
}
)";

constexpr std::array<const char*, 4> kShadowTextureFallbackRoots = {
    "resource/textures/shadow_map.png",
    "../resource/textures/shadow_map.png",
    "resource/textures/8k_moon.jpg",
    "../resource/textures/8k_moon.jpg",
};

std::optional<std::string> resolve_shadow_texture_path() {
    for (const auto* path : kShadowTextureFallbackRoots) {
        if (fs::exists(path)) {
            return std::string(path);
        }
    }
    return std::nullopt;
}

} // namespace

std::optional<fs::path> ForwardPlusRenderer::resolve_shader_path(const char* filename, bool track_missing) {
    for (const auto* root : kShaderSearchRoots) {
        const fs::path candidate = fs::path(root) / filename;
        if (fs::exists(candidate)) {
            return candidate;
        }
    }
    if (track_missing && std::find(missing_shaders_.begin(), missing_shaders_.end(), filename) == missing_shaders_.end()) {
        missing_shaders_.push_back(filename);
    }
    return std::nullopt;
}

bool ForwardPlusRenderer::load_shader_pair(const char* vert_file, const char* frag_file, const char* mesh_file,
    ShaderModulePack& pack)
{
    const auto mesh_path = mesh_file ? resolve_shader_path(mesh_file, false) : std::optional<fs::path>{};
    const bool prefer_mesh = mesh_path.has_value();

    const auto vert_path = prefer_mesh ? std::optional<fs::path>{} : resolve_shader_path(vert_file);
    const auto frag_path = resolve_shader_path(frag_file);
    if ((!prefer_mesh && !vert_path) || !frag_path) {
        return false;
    }

    ShaderModule stage0;
    ShaderModule frag;
    if (prefer_mesh) {
        if (!stage0.load(*mesh_path, vk::ShaderStageFlagBits::eMeshEXT)) {
            return false;
        }
    }
    else {
        if (!stage0.load(*vert_path, vk::ShaderStageFlagBits::eVertex)) {
            return false;
        }
    }
    if (!frag.load(*frag_path, vk::ShaderStageFlagBits::eFragment)) {
        return false;
    }

    if (frag.img_infos.find("shadowMap") != frag.img_infos.end()) {
        const auto shadow_map_path = resolve_shadow_texture_path();
        if (!shadow_map_path) {
            return false;
        }
        frag.tex_img_pairs["shadowMap"] = std::make_pair(*shadow_map_path, false);
    }

    return pack.add_shader_module(stage0, true) && pack.add_shader_module(frag, true);
}

bool ForwardPlusRenderer::create_shader_pipeline(const char* pipeline_name,
    const char* vert_file, const char* frag_file, const char* mesh_file,
    const std::vector<VERT_COMP>& components, PipelineOption& option)
{
    if (!ctx) {
        return false;
    }

    ShaderModulePack pack;
    if (!load_shader_pair(vert_file, frag_file, mesh_file, pack)) {
        return false;
    }
    return ctx->create_pipeline(pipeline_name, pack, option, components);
}

bool ForwardPlusRenderer::create_shadow_pipeline() {
    if (!ctx) {
        return false;
    }

    PipelineOption shadow_option;
    shadow_option.setup_multisampling(ctx->sample_rate_shading_enabled, ctx->nsample);
    shadow_option.setup_rasterizer(false, false, vk::PolygonMode::eFill, 1.0f,
        vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise, false);
    shadow_option.setup_depth_stencil(true, true, vk::CompareOp::eLessOrEqual, false, false);
    shadow_option.setup_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);
    shadow_option.setup_scissor(0, 0, width, height);
    shadow_option.blend_attachment_info.colorWriteMask = {};

    ShaderModulePack pack;
    if (!load_shader_pair(kShaderShadowVert, kShaderShadowFrag, nullptr, pack)) {
        return false;
    }
    return ctx->create_pipeline(kShadowPipeline, pack, shadow_option, {VERTEX});
}

bool ForwardPlusRenderer::initialize(Context* context) {
    ctx = context;
    if (!ctx) {
        return false;
    }

    missing_shaders_.clear();
    if (ctx->get_window()) {
        int w = 0;
        int h = 0;
        glfwGetFramebufferSize(ctx->get_window(), &w, &h);
        width = static_cast<uint32_t>(std::max(w, 1));
        height = static_cast<uint32_t>(std::max(h, 1));
    }

    return create_pass_pipelines();
}

void ForwardPlusRenderer::shutdown() {
    destroy_pass_pipelines();
    draw_items_.clear();
    ctx = nullptr;
    scene = nullptr;
    camera = nullptr;
}

void ForwardPlusRenderer::add_draw_item(const ForwardPlusDrawItem& item) {
    if (!item.pipeline_name.empty()) {
        ensure_draw_item_pipeline(item);
    }
    draw_items_.push_back(item);
}

bool ForwardPlusRenderer::ensure_draw_item_pipeline(const ForwardPlusDrawItem& item) {
    if (!ctx || item.pipeline_name.empty()) {
        return false;
    }
    if (ctx->pipelines.find(item.pipeline_name) != ctx->pipelines.end()) {
        return true;
    }

    const std::vector<VERT_COMP> components{VERTEX, NORMAL};
    auto option = item.transparent
        ? make_transparent_pipeline_option()
        : make_base_pipeline_option();
    if (item.transparent) {
        return create_shader_pipeline(item.pipeline_name.c_str(),
            kShaderTransparentVert, kShaderTransparentFrag, kShaderTransparentMesh, components, option);
    }
    if (opaque_pipeline_uses_shadow_) {
        return create_shader_pipeline(item.pipeline_name.c_str(),
            kShaderOpaqueShadowVert, kShaderOpaqueShadowFrag, nullptr, components, option);
    }
    return create_shader_pipeline(item.pipeline_name.c_str(),
        kShaderOpaqueFallbackVert, kShaderOpaqueFallbackFrag, kShaderOpaqueMesh, components, option);
}

void ForwardPlusRenderer::clear_draw_items() {
    draw_items_.clear();
}

void ForwardPlusRenderer::on_resize(uint32_t width, uint32_t height) {
    this->width = width;
    this->height = height;
}

PipelineOption ForwardPlusRenderer::make_base_pipeline_option() const {
    PipelineOption option;
    option.setup_multisampling(ctx->sample_rate_shading_enabled, ctx->nsample);
    option.setup_rasterizer(false, false, vk::PolygonMode::eFill, 1.0f,
        vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise, false);
    option.setup_depth_stencil(true, true, vk::CompareOp::eLess, false, false);
    option.setup_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);
    option.setup_scissor(0, 0, width, height);
    return option;
}

PipelineOption ForwardPlusRenderer::make_transparent_pipeline_option() const {
    auto option = make_base_pipeline_option();
    option.setup_depth_stencil(true, false, vk::CompareOp::eLess, false, false);
    option.blend_attachment_info.blendEnable = vk::True;
    option.blend_attachment_info.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    option.blend_attachment_info.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    option.blend_attachment_info.colorBlendOp = vk::BlendOp::eAdd;
    option.blend_attachment_info.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    option.blend_attachment_info.dstAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    option.blend_attachment_info.alphaBlendOp = vk::BlendOp::eAdd;
    return option;
}

bool ForwardPlusRenderer::create_pass_pipelines() {
    if (!ctx) {
        return false;
    }

    if (!create_shadow_pipeline()) {
        return false;
    }

    const std::vector<VERT_COMP> phong_components{VERTEX, NORMAL};
    auto opaque_option = make_base_pipeline_option();
    opaque_pipeline_uses_shadow_ = create_shader_pipeline(kOpaquePipeline,
        kShaderOpaqueShadowVert, kShaderOpaqueShadowFrag, nullptr, phong_components, opaque_option);
    if (!opaque_pipeline_uses_shadow_) {
        if (!create_shader_pipeline(kOpaquePipeline,
                kShaderOpaqueFallbackVert, kShaderOpaqueFallbackFrag, kShaderOpaqueMesh, phong_components, opaque_option))
        {
            return false;
        }
    }

    auto transparent_option = make_transparent_pipeline_option();
    if (!create_shader_pipeline(kTransparentPipeline,
            kShaderTransparentVert, kShaderTransparentFrag, kShaderTransparentMesh,
            phong_components, transparent_option))
    {
        return false;
    }

    return true;
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

void ForwardPlusRenderer::draw_batch(vk::CommandBuffer cmd, const RenderView& view,
    const std::vector<const ForwardPlusDrawItem*>& items, const char* fallback_pipeline)
{
    if (!ctx) {
        return;
    }

    for (const auto* item : items) {
        if (!item) {
            continue;
        }

        const std::string& pipeline_name = item->pipeline_name.empty()
            ? fallback_pipeline
            : item->pipeline_name;

        const auto pipeline_found = ctx->pipelines.find(pipeline_name);
        if (pipeline_found == ctx->pipelines.end()) {
            continue;
        }

        const auto& pipeline = pipeline_found->second;
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline.vk_pipeline);

        sync_draw_item_uniforms(view.swapchain_image_idx, *item, pipeline_name);

        if (pipeline.uses_mesh_shader) {
            if (view.swapchain_image_idx < pipeline.descriptor_sets.size()) {
                const vk::DescriptorSet desc_set = *pipeline.descriptor_sets[view.swapchain_image_idx];
                cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipeline.vk_pipeline_layout, 0, {desc_set}, {});
            }
            ctx->draw_mesh_tasks(cmd, 1, 1, 1);
            continue;
        }

        const auto mesh_found = ctx->meshes.find(item->mesh_name);
        if (mesh_found == ctx->meshes.end()) {
            continue;
        }

        const vk::DescriptorSet* desc_set = nullptr;
        if (view.swapchain_image_idx < pipeline.descriptor_sets.size()) {
            desc_set = &*pipeline.descriptor_sets[view.swapchain_image_idx];
        }
        mesh_found->second.emit_draw_cmd(cmd, *pipeline.vk_pipeline_layout, desc_set);
    }
}

void ForwardPlusRenderer::pass_opaque(vk::CommandBuffer cmd, const RenderView& view) {
    std::vector<const ForwardPlusDrawItem*> opaque_items;
    opaque_items.reserve(draw_items_.size());
    for (const auto& item : draw_items_) {
        if (!item.transparent) {
            opaque_items.push_back(&item);
        }
    }
    draw_batch(cmd, view, opaque_items, kOpaquePipeline);
}

void ForwardPlusRenderer::pass_transparent(vk::CommandBuffer cmd, const RenderView& view) {
    std::vector<const ForwardPlusDrawItem*> transparent_items;
    for (const auto& item : draw_items_) {
        if (item.transparent) {
            transparent_items.push_back(&item);
        }
    }

    if (!camera || transparent_items.empty()) {
        draw_batch(cmd, view, transparent_items, kTransparentPipeline);
        return;
    }

    std::sort(transparent_items.begin(), transparent_items.end(),
        [this](const ForwardPlusDrawItem* a, const ForwardPlusDrawItem* b) {
            const glm::vec3 a_center = glm::vec3(a->model * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            const glm::vec3 b_center = glm::vec3(b->model * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            const glm::vec3 to_a = a_center - camera->pos;
            const glm::vec3 to_b = b_center - camera->pos;
            return glm::dot(to_a, to_a) > glm::dot(to_b, to_b);
        });

    draw_batch(cmd, view, transparent_items, kTransparentPipeline);
}

void ForwardPlusRenderer::record_commands(VkCommandBuffer cmd, const RenderView& view) {
    record_commands(vk::CommandBuffer(cmd), view);
}

void ForwardPlusRenderer::record_commands(vk::CommandBuffer cmd, const RenderView& view) {
    if (!ctx) {
        return;
    }

    pass_shadow(cmd, view);
    pass_opaque(cmd, view);
    pass_transparent(cmd, view);
    if (overlay_draw_) {
        overlay_draw_(cmd);
    }
}

} // namespace vkkk
