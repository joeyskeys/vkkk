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
    if (!ctx_) {
        return false;
    }

    ShaderModulePack pack;
    if (!load_shader_pair(vert_file, frag_file, pack)) {
        return false;
    }
    return ctx_->create_pipeline(pipeline_name, pack, option, components);
}

bool ForwardPlusRenderer::initialize(VkWrappedInstance* instance) {
    (void)instance;
    // Old wrapper path intentionally unsupported.
    return false;
}

bool ForwardPlusRenderer::initialize(Context* context) {
    ctx_ = context;
    if (!ctx_) {
        return false;
    }

    missing_shaders_.clear();
    if (ctx_->get_window()) {
        int w = 0;
        int h = 0;
        glfwGetFramebufferSize(ctx_->get_window(), &w, &h);
        width_ = static_cast<uint32_t>(std::max(w, 1));
        height_ = static_cast<uint32_t>(std::max(h, 1));
    }

    return create_pass_pipelines();
}

void ForwardPlusRenderer::shutdown() {
    destroy_pass_pipelines();
    draw_items_.clear();
    ctx_ = nullptr;
    scene_ = nullptr;
    camera_ = nullptr;
}

void ForwardPlusRenderer::set_scene(Scene* scene) {
    scene_ = scene;
}

void ForwardPlusRenderer::add_draw_item(const ForwardPlusDrawItem& item) {
    if (!item.pipeline_name.empty()) {
        ensure_draw_item_pipeline(item);
    }
    draw_items_.push_back(item);
}

bool ForwardPlusRenderer::ensure_draw_item_pipeline(const ForwardPlusDrawItem& item) {
    if (!ctx_ || item.pipeline_name.empty()) {
        return false;
    }
    if (ctx_->pipelines.find(item.pipeline_name) != ctx_->pipelines.end()) {
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
    width_ = width;
    height_ = height;
}

PipelineOption ForwardPlusRenderer::make_base_pipeline_option() const {
    PipelineOption option;
    option.setup_multisampling(true, ctx_->nsample);
    option.setup_rasterizer(false, false, vk::PolygonMode::eFill, 1.0f,
        vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise, false);
    option.setup_depth_stencil(true, true, vk::CompareOp::eLess, false, false);
    option.setup_viewport(0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_), 0.0f, 1.0f);
    option.setup_scissor(0, 0, width_, height_);
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
    if (!ctx_) {
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
    if (!camera_ || height_ == 0) {
        return;
    }
    camera_->ratio = width_ / static_cast<float>(height_);
}

void ForwardPlusRenderer::update_lights_from_scene() {
    light_ubo_.lightPos = glm::vec4(0.0f, 5.0f, 5.0f, 1.0f);
    light_ubo_.lightColor = glm::vec4(1.0f);

    if (scene_ && scene_->light_mgr) {
        const auto& pt_lights = scene_->light_mgr->point_lights();
        if (!pt_lights.empty()) {
            light_ubo_.lightPos = pt_lights[0].pos;
            light_ubo_.lightColor = pt_lights[0].color;
        }
    }

    light_ubo_.viewPos = camera_
        ? glm::vec4(camera_->pos, 1.0f)
        : glm::vec4(0.0f, 0.0f, 5.0f, 1.0f);
}

void ForwardPlusRenderer::update_global_uniforms(uint32_t swapchain_idx) {
    if (!ctx_ || !camera_) {
        return;
    }

    PhongTransformUBO camera_transform{};
    camera_transform.model = glm::mat4(1.0f);
    camera_transform.view = camera_->get_view_mat();
    camera_transform.proj = camera_->get_proj_mat();

    auto sync_global_ubo = [&](const char* pipeline_name, const char* suffix,
                               const void* data, size_t size) {
        const auto ubo_name = std::string(pipeline_name) + suffix;
        auto found = ctx_->ubos.find(ubo_name);
        if (found == ctx_->ubos.end() || swapchain_idx >= found->second.memos.size()) {
            return;
        }
        ctx_->sync_uniform(found->second.memos[swapchain_idx], data, static_cast<uint32_t>(size));
    };

    for (const auto* pipeline_name : {kOpaquePipeline, kTransparentPipeline}) {
        sync_global_ubo(pipeline_name, ":UniformBufferObject", &camera_transform, sizeof(camera_transform));
        sync_global_ubo(pipeline_name, ":PhongLight", &light_ubo_, sizeof(light_ubo_));
    }
}

void ForwardPlusRenderer::sync_draw_item_uniforms(uint32_t swapchain_idx,
    const ForwardPlusDrawItem& item, const std::string& pipeline_name)
{
    if (!ctx_ || !camera_ || pipeline_name.empty()) {
        return;
    }

    PhongTransformUBO transform_ubo{};
    transform_ubo.model = item.model;
    transform_ubo.view = camera_->get_view_mat();
    transform_ubo.proj = camera_->get_proj_mat();

    auto sync_if_present = [&](const std::string& ubo_suffix, const void* data, size_t size) {
        const auto ubo_name = pipeline_name + ubo_suffix;
        auto found = ctx_->ubos.find(ubo_name);
        if (found == ctx_->ubos.end() || swapchain_idx >= found->second.memos.size()) {
            return;
        }
        ctx_->sync_uniform(found->second.memos[swapchain_idx], data, static_cast<uint32_t>(size));
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
    if (!ctx_) {
        return;
    }

    for (const auto* item : items) {
        if (!item) {
            continue;
        }

        const std::string& pipeline_name = item->pipeline_name.empty()
            ? fallback_pipeline
            : item->pipeline_name;

        const auto mesh_found = ctx_->meshes.find(item->mesh_name);
        if (mesh_found == ctx_->meshes.end()) {
            continue;
        }

        const auto pipeline_found = ctx_->pipelines.find(pipeline_name);
        if (pipeline_found == ctx_->pipelines.end()) {
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

    if (!camera_ || transparent_items.empty()) {
        draw_batch(cmd, view, transparent_items, kTransparentPipeline);
        return;
    }

    std::sort(transparent_items.begin(), transparent_items.end(),
        [this](const ForwardPlusDrawItem* a, const ForwardPlusDrawItem* b) {
            const glm::vec3 a_center = glm::vec3(a->model * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            const glm::vec3 b_center = glm::vec3(b->model * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            const glm::vec3 to_a = a_center - camera_->pos;
            const glm::vec3 to_b = b_center - camera_->pos;
            return glm::dot(to_a, to_a) > glm::dot(to_b, to_b);
        });

    draw_batch(cmd, view, transparent_items, kTransparentPipeline);
}

void ForwardPlusRenderer::record_commands(VkCommandBuffer cmd, const RenderView& view) {
    record_commands(vk::CommandBuffer(cmd), view);
}

void ForwardPlusRenderer::record_commands(vk::CommandBuffer cmd, const RenderView& view) {
    if (!ctx_) {
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
