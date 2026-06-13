#include "renderer/forward_plus.h"

#include <algorithm>
#include <array>
#include <utility>

#include <glm/gtc/matrix_transform.hpp>

#include "asset_mgr/light_mgr.h"
#include "asset_mgr/scene.h"
#include "concepts/camera.h"

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

} // namespace

std::optional<fs::path> ForwardPlusRenderer::resolve_shader_path(const char* filename) {
    for (const auto* root : kShaderSearchRoots) {
        const fs::path candidate = fs::path(root) / filename;
        if (fs::exists(candidate)) {
            return candidate;
        }
    }
    if (std::find(missing_shaders_.begin(), missing_shaders_.end(), filename) == missing_shaders_.end()) {
        missing_shaders_.push_back(filename);
    }
    return std::nullopt;
}

bool ForwardPlusRenderer::load_shader_pair(const char* vert_file, const char* frag_file,
    ShaderModulePack& pack)
{
    const auto vert_path = resolve_shader_path(vert_file);
    const auto frag_path = resolve_shader_path(frag_file);
    if (!vert_path || !frag_path) {
        return false;
    }

    ShaderModule vert;
    ShaderModule frag;
    if (!vert.load(*vert_path, vk::ShaderStageFlagBits::eVertex)) {
        return false;
    }
    if (!frag.load(*frag_path, vk::ShaderStageFlagBits::eFragment)) {
        return false;
    }

    return pack.add_shader_module(vert, true) && pack.add_shader_module(frag, true);
}

bool ForwardPlusRenderer::create_shader_pipeline(const char* pipeline_name,
    const char* vert_file, const char* frag_file,
    const std::vector<VERT_COMP>& components, PipelineOption& option)
{
    if (!ctx) {
        return false;
    }

    ShaderModulePack pack;
    if (!load_shader_pair(vert_file, frag_file, pack)) {
        return false;
    }
    return ctx->create_pipeline(pipeline_name, pack, option, components);
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
            kShaderTransparentVert, kShaderTransparentFrag, components, option);
    }
    return create_shader_pipeline(item.pipeline_name.c_str(),
        kShaderOpaqueVert, kShaderOpaqueFrag, components, option);
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

    const std::vector<VERT_COMP> phong_components{VERTEX, NORMAL};
    auto opaque_option = make_base_pipeline_option();
    if (!create_shader_pipeline(kOpaquePipeline,
            kShaderOpaqueVert, kShaderOpaqueFrag, phong_components, opaque_option))
    {
        return false;
    }

    auto transparent_option = make_transparent_pipeline_option();
    if (!create_shader_pipeline(kTransparentPipeline,
            kShaderTransparentVert, kShaderTransparentFrag,
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
}

void ForwardPlusRenderer::prepare_light_clusters(const RenderView& view) {
    (void)view;
    // TODO(forward-plus): build cluster grid and cull lights in compute pass.
}

void ForwardPlusRenderer::pass_shadow_placeholder(vk::CommandBuffer cmd, const RenderView& view) {
    (void)cmd;
    (void)view;
    // TODO(shadow): add shadow-map render pass once infrastructure is ready.
}

void ForwardPlusRenderer::update(const RenderView& view) {
    update_camera_aspect();
    update_lights_from_scene();
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

        const auto mesh_found = ctx->meshes.find(item->mesh_name);
        if (mesh_found == ctx->meshes.end()) {
            continue;
        }

        const auto pipeline_found = ctx->pipelines.find(pipeline_name);
        if (pipeline_found == ctx->pipelines.end()) {
            continue;
        }

        const auto& pipeline = pipeline_found->second;
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline.vk_pipeline);

        sync_draw_item_uniforms(view.swapchain_image_idx, *item, pipeline_name);

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

    pass_shadow_placeholder(cmd, view);
    pass_opaque(cmd, view);
    pass_transparent(cmd, view);
    if (overlay_draw_) {
        overlay_draw_(cmd);
    }
}

} // namespace vkkk
