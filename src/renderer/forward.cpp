#include "renderer/forward.h"

#include <algorithm>
#include <array>

#include <glm/gtc/matrix_transform.hpp>

#include "asset_mgr/light_mgr.h"
#include "asset_mgr/scene.h"
#include "concepts/camera.h"
#include "vk_ins/vkabstraction.h"

namespace vkkk
{

namespace
{

using built_in_shader::BuiltInShaderType;
using built_in_shader::PhongLightUBO;
using built_in_shader::PhongMaterialUBO;
using built_in_shader::PhongTransformUBO;

constexpr std::array<const char*, 4> kShaderSearchRoots = {
    "resource/shaders/forward/",
    "../resource/shaders/forward/",
    "../../resource/shaders/forward/",
    "../../../resource/shaders/forward/",
};

} // namespace

std::optional<fs::path> ForwardRenderer::resolve_shader_path(const char* filename) {
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

bool ForwardRenderer::load_shader_pair(const char* vert_file, const char* frag_file,
    std::vector<ShaderModule>& modules)
{
    const auto vert_path = resolve_shader_path(vert_file);
    const auto frag_path = resolve_shader_path(frag_file);
    if (!vert_path || !frag_path) {
        return false;
    }

    modules.resize(2);
    if (!modules[0].load(*vert_path, VK_SHADER_STAGE_VERTEX_BIT)) {
        return false;
    }
    return modules[1].load(*frag_path, VK_SHADER_STAGE_FRAGMENT_BIT);
}

bool ForwardRenderer::create_shader_pipeline(const char* pipeline_name,
    const char* vert_file, const char* frag_file,
    const std::vector<VERT_COMP>& components, PipelineOption& option,
    const VkRenderPass render_pass_override)
{
    std::vector<ShaderModule> modules;
    if (!load_shader_pair(vert_file, frag_file, modules)) {
        return false;
    }
    return ins_->create_pipeline(pipeline_name, modules, components, option, render_pass_override);
}

bool ForwardRenderer::initialize(VkWrappedInstance* instance) {
    ins_ = instance;
    if (!ins_) {
        return false;
    }

    shader_mgr_ = std::make_unique<built_in_shader::BuiltInShaderMgr>(ins_);
    width_ = ins_->get_swapchain_extent().width;
    height_ = ins_->get_swapchain_extent().height;
    missing_shaders_.clear();

    shadow_target_ready_ = ensure_shadow_resources();
    shadow_pipeline_ready_ = create_shadow_pass_objects();

    return create_pass_pipelines();
}

void ForwardRenderer::shutdown() {
    destroy_pass_pipelines();
    destroy_shadow_pass_objects();
    draw_items_.clear();
    shader_mgr_.reset();
    ins_ = nullptr;
    scene_ = nullptr;
    camera_ = nullptr;
    shadow_pipeline_ready_ = false;
    post_pipeline_ready_ = false;
    shadow_target_ready_ = false;
}

void ForwardRenderer::set_scene(Scene* scene) {
    scene_ = scene;
}

void ForwardRenderer::add_draw_item(const ForwardDrawItem& item) {
    if (!item.pipeline_name.empty()) {
        ensure_draw_item_pipeline(item);
    }
    draw_items_.push_back(item);
}

bool ForwardRenderer::ensure_draw_item_pipeline(const ForwardDrawItem& item) {
    if (!ins_ || item.pipeline_name.empty()) {
        return false;
    }
    if (ins_->pipelines.find(item.pipeline_name) != ins_->pipelines.end()) {
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

    if (!create_shader_pipeline(item.pipeline_name.c_str(),
            kShaderOpaqueShadowVert, kShaderOpaqueShadowFrag, components, option))
    {
        return false;
    }
    return bind_shadow_map_texture(item.pipeline_name.c_str());
}

void ForwardRenderer::clear_draw_items() {
    draw_items_.clear();
}

void ForwardRenderer::on_resize(uint32_t width, uint32_t height) {
    width_ = width;
    height_ = height;
    destroy_shadow_pass_objects();
    shadow_target_ready_ = ensure_shadow_resources();
    shadow_pipeline_ready_ = create_shadow_pass_objects();
}

PipelineOption ForwardRenderer::make_base_pipeline_option() const {
    PipelineOption option;
    option.setup_multisampling(true, ins_->nsample);
    option.setup_rasterizer(false, false, VK_POLYGON_MODE_FILL, 1.0f,
        VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, false);
    option.setup_depth_stencil(true, true, VK_COMPARE_OP_LESS, false, false);
    option.setup_viewport(0.0f, 0.0f,
        static_cast<float>(ins_->get_swapchain_extent().width),
        static_cast<float>(ins_->get_swapchain_extent().height),
        0.0f, 1.0f);
    option.setup_scissor(0, 0,
        ins_->get_swapchain_extent().width,
        ins_->get_swapchain_extent().height);
    return option;
}

PipelineOption ForwardRenderer::make_transparent_pipeline_option() const {
    auto option = make_base_pipeline_option();
    option.setup_depth_stencil(true, false, VK_COMPARE_OP_LESS, false, false);
    option.blend_attachment.blendEnable = VK_TRUE;
    option.blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    option.blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    option.blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    option.blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    option.blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    option.blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    return option;
}

PipelineOption ForwardRenderer::make_shadow_pipeline_option() const {
    PipelineOption option;
    option.setup_multisampling(false, VK_SAMPLE_COUNT_1_BIT);
    option.setup_rasterizer(false, false, VK_POLYGON_MODE_FILL, 1.0f,
        VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, true);
    option.setup_depth_stencil(true, true, VK_COMPARE_OP_LESS_OR_EQUAL, false, false);
    option.setup_viewport(0.0f, 0.0f,
        static_cast<float>(width_),
        static_cast<float>(height_),
        0.0f, 1.0f);
    option.setup_scissor(0, 0, width_, height_);
    option.rasterizer.depthBiasConstantFactor = 1.25f;
    option.rasterizer.depthBiasSlopeFactor = 1.75f;
    return option;
}

PipelineOption ForwardRenderer::make_post_pipeline_option() const {
    PipelineOption option;
    // Must match the MSAA swapchain render pass (same as opaque geometry).
    option.setup_multisampling(true, ins_->nsample);
    option.setup_rasterizer(false, false, VK_POLYGON_MODE_FILL, 1.0f,
        VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, false);
    option.setup_depth_stencil(false, false, VK_COMPARE_OP_ALWAYS, false, false);
    option.setup_viewport(0.0f, 0.0f,
        static_cast<float>(ins_->get_swapchain_extent().width),
        static_cast<float>(ins_->get_swapchain_extent().height),
        0.0f, 1.0f);
    option.setup_scissor(0, 0,
        ins_->get_swapchain_extent().width,
        ins_->get_swapchain_extent().height);
    return option;
}

bool ForwardRenderer::create_pass_pipelines() {
    if (!shader_mgr_) {
        return false;
    }

    const std::vector<VERT_COMP> phong_components{VERTEX, NORMAL};
    auto opaque_option = make_base_pipeline_option();
    if (!create_shader_pipeline(kOpaquePipeline,
            kShaderOpaqueShadowVert, kShaderOpaqueShadowFrag, phong_components, opaque_option))
    {
        return false;
    }
    if (!bind_shadow_map_texture(kOpaquePipeline)) {
        return false;
    }

    auto transparent_option = make_transparent_pipeline_option();
    if (!create_shader_pipeline(kTransparentPipeline,
            kShaderTransparentVert, kShaderTransparentFrag,
            phong_components, transparent_option))
    {
        return false;
    }

    auto post_option = make_post_pipeline_option();
    post_pipeline_ready_ = create_shader_pipeline(kPostPipeline,
        kShaderPostVert, kShaderPostFrag, {}, post_option);
    if (post_pipeline_ready_) {
        bind_scene_color_texture(kPostPipeline);
        const auto swapchain_cnt = ins_->get_swapchain_cnt();
        for (uint32_t i = 0; i < swapchain_cnt; ++i) {
            auto& post_ubo = ins_->require_ubo(std::string(kPostPipeline) + ":post");
            ins_->sync_uniform(post_ubo.memos[i], &post_params_ubo_, sizeof(post_params_ubo_));
        }
    }

    return true;
}

void ForwardRenderer::destroy_pass_pipelines() {
    // Pipelines are owned by VkWrappedInstance and torn down with the instance.
}

bool ForwardRenderer::ensure_shadow_resources() {
    if (!ins_) {
        return false;
    }

    if (ins_->render_targets.find(kShadowMapTarget) != ins_->render_targets.end()) {
        return true;
    }

    const auto depth_format = ins_->find_depth_format();
    return ins_->create_render_target(
        kShadowMapTarget,
        depth_format,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT);
}

bool ForwardRenderer::create_shadow_pass_objects() {
    if (!ins_ || !shadow_target_ready_) {
        return false;
    }

    auto target_found = ins_->render_targets.find(kShadowMapTarget);
    if (target_found == ins_->render_targets.end()) {
        return false;
    }

    const auto depth_format = target_found->second.format;

    VkAttachmentDescription depth_attachment{};
    depth_attachment.format = depth_format;
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depth_ref{};
    depth_ref.attachment = 0;
    depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.pDepthStencilAttachment = &depth_ref;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo pass_info{};
    pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    pass_info.attachmentCount = 1;
    pass_info.pAttachments = &depth_attachment;
    pass_info.subpassCount = 1;
    pass_info.pSubpasses = &subpass;
    pass_info.dependencyCount = 1;
    pass_info.pDependencies = &dependency;

    if (vkCreateRenderPass(ins_->get_device(), &pass_info, nullptr, &shadow_render_pass_) != VK_SUCCESS) {
        return false;
    }

    VkFramebufferCreateInfo fb_info{};
    fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_info.renderPass = shadow_render_pass_;
    fb_info.attachmentCount = 1;
    fb_info.pAttachments = &target_found->second.view;
    fb_info.width = width_;
    fb_info.height = height_;
    fb_info.layers = 1;

    if (vkCreateFramebuffer(ins_->get_device(), &fb_info, nullptr, &shadow_framebuffer_) != VK_SUCCESS) {
        vkDestroyRenderPass(ins_->get_device(), shadow_render_pass_, nullptr);
        shadow_render_pass_ = VK_NULL_HANDLE;
        return false;
    }

    auto shadow_option = make_shadow_pipeline_option();
    shadow_pipeline_ready_ = create_shader_pipeline(kShadowPipeline,
        kShaderShadowDepthVert, kShaderShadowDepthFrag,
        {VERTEX}, shadow_option, shadow_render_pass_);

    return shadow_pipeline_ready_;
}

void ForwardRenderer::destroy_shadow_pass_objects() {
    if (!ins_) {
        shadow_framebuffer_ = VK_NULL_HANDLE;
        shadow_render_pass_ = VK_NULL_HANDLE;
        shadow_pipeline_ready_ = false;
        return;
    }

    if (shadow_framebuffer_ != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(ins_->get_device(), shadow_framebuffer_, nullptr);
        shadow_framebuffer_ = VK_NULL_HANDLE;
    }
    if (shadow_render_pass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(ins_->get_device(), shadow_render_pass_, nullptr);
        shadow_render_pass_ = VK_NULL_HANDLE;
    }
    shadow_pipeline_ready_ = false;
}

bool ForwardRenderer::bind_shadow_map_texture(const char* pipeline_name) {
    auto target_found = ins_->render_targets.find(kShadowMapTarget);
    if (target_found == ins_->render_targets.end()) {
        return false;
    }

    const auto tex_name = std::string(pipeline_name) + ":shadowMap";
    if (ins_->textures.find(tex_name) != ins_->textures.end()) {
        return true;
    }

    return ins_->add_sampled_image(
        tex_name,
        3,
        target_found->second.view,
        target_found->second.format,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        true);
}

bool ForwardRenderer::bind_scene_color_texture(const char* pipeline_name) {
    const auto tex_name = std::string(pipeline_name) + ":sceneColor";
    if (ins_->textures.find(tex_name) != ins_->textures.end()) {
        return true;
    }

    const std::array<const char*, 2> scene_paths = {
        "resource/textures/8k_moon.jpg",
        "../resource/textures/8k_moon.jpg",
    };
    for (const auto* path : scene_paths) {
        if (fs::exists(path)) {
            return ins_->add_texture(tex_name, 1, path);
        }
    }
    return false;
}

glm::mat4 ForwardRenderer::compute_light_space_matrix() const {
    const glm::vec3 light_pos = glm::vec3(light_ubo_.lightPos);
    const glm::mat4 light_view = glm::lookAt(
        light_pos,
        glm::vec3(0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 light_proj = glm::ortho(-6.0f, 6.0f, -6.0f, 6.0f, 0.1f, 30.0f);
    return light_proj * light_view;
}

void ForwardRenderer::update_camera_aspect() {
    if (!camera_ || !ins_) {
        return;
    }
    const auto& extent = ins_->get_swapchain_extent();
    if (extent.height > 0) {
        camera_->ratio = extent.width / static_cast<float>(extent.height);
    }
}

void ForwardRenderer::update_lights_from_scene() {
    light_ubo_.lightPos = glm::vec4(0.0f, 5.0f, 5.0f, 1.0f);
    light_ubo_.lightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

    if (scene_ && scene_->light_mgr) {
        const auto& pt_lights = scene_->light_mgr->point_lights();
        if (!pt_lights.empty()) {
            light_ubo_.lightPos = pt_lights[0].pos;
            light_ubo_.lightColor = pt_lights[0].color;
        }
        else {
            const auto& dir_lights = scene_->light_mgr->directional_lights();
            if (!dir_lights.empty()) {
                const glm::vec3 dir = glm::vec3(dir_lights[0].direction);
                light_ubo_.lightPos = glm::vec4(-dir, 0.0f);
                light_ubo_.lightColor = dir_lights[0].color;
            }
        }
    }

    light_ubo_.viewPos = camera_
        ? glm::vec4(camera_->pos, 1.0f)
        : glm::vec4(0.0f, 0.0f, 5.0f, 1.0f);

    shadow_params_ubo_.lightSpace = compute_light_space_matrix();
}

void ForwardRenderer::update_global_uniforms(uint32_t swapchain_idx) {
    if (post_pipeline_ready_) {
        const auto post_ubo_name = std::string(kPostPipeline) + ":post";
        auto found = ins_->ubos.find(post_ubo_name);
        if (found != ins_->ubos.end() && swapchain_idx < found->second.memos.size()) {
            ins_->sync_uniform(found->second.memos[swapchain_idx], &post_params_ubo_,
                sizeof(post_params_ubo_));
        }
    }

    if (!camera_) {
        return;
    }

    PhongTransformUBO camera_transform{};
    camera_transform.model = glm::mat4(1.0f);
    camera_transform.view = camera_->get_view_mat();
    camera_transform.proj = camera_->get_proj_mat();

    auto sync_global_ubo = [&](const char* pipeline_name, const char* suffix,
                               const void* data, size_t size) {
        const auto ubo_name = std::string(pipeline_name) + suffix;
        auto found = ins_->ubos.find(ubo_name);
        if (found == ins_->ubos.end() || swapchain_idx >= found->second.memos.size()) {
            return;
        }
        ins_->sync_uniform(found->second.memos[swapchain_idx], data, size);
    };

    for (const auto* pipeline_name : {kOpaquePipeline, kTransparentPipeline}) {
        sync_global_ubo(pipeline_name, ":UniformBufferObject", &camera_transform, sizeof(camera_transform));
        sync_global_ubo(pipeline_name, ":PhongLight", &light_ubo_, sizeof(light_ubo_));

        if (pipeline_name == kOpaquePipeline) {
            sync_global_ubo(pipeline_name, ":ShadowParams", &shadow_params_ubo_, sizeof(shadow_params_ubo_));
        }
    }

    refresh_shadow_map_descriptors(swapchain_idx);
}

void ForwardRenderer::sync_draw_item_uniforms(uint32_t swapchain_idx,
    const ForwardDrawItem& item, const std::string& pipeline_name)
{
    if (!camera_ || pipeline_name.empty()) {
        return;
    }

    PhongTransformUBO transform_ubo{};
    transform_ubo.model = item.model;
    transform_ubo.view = camera_->get_view_mat();
    transform_ubo.proj = camera_->get_proj_mat();

    auto sync_if_present = [&](const std::string& ubo_suffix, const void* data, size_t size) {
        const auto ubo_name = pipeline_name + ubo_suffix;
        auto found = ins_->ubos.find(ubo_name);
        if (found == ins_->ubos.end() || swapchain_idx >= found->second.memos.size()) {
            return;
        }
        ins_->sync_uniform(found->second.memos[swapchain_idx], data, size);
    };

    sync_if_present(":ubo", &transform_ubo, sizeof(transform_ubo));
    sync_if_present(":UniformBufferObject", &transform_ubo, sizeof(transform_ubo));
    sync_if_present(":material", &item.material, sizeof(item.material));
    sync_if_present(":PhongMaterial", &item.material, sizeof(item.material));
    sync_if_present(":light", &light_ubo_, sizeof(light_ubo_));
    sync_if_present(":PhongLight", &light_ubo_, sizeof(light_ubo_));
    sync_if_present(":ShadowParams", &shadow_params_ubo_, sizeof(shadow_params_ubo_));
}

void ForwardRenderer::refresh_shadow_map_descriptors(uint32_t swapchain_idx) {
    if (!shadow_target_ready_) {
        return;
    }

    const auto shadow_tex_name = std::string(kOpaquePipeline) + ":shadowMap";
    auto tex_found = ins_->textures.find(shadow_tex_name);
    auto target_found = ins_->render_targets.find(kShadowMapTarget);
    auto pipeline_found = ins_->pipelines.find(kOpaquePipeline);
    if (tex_found == ins_->textures.end() || target_found == ins_->render_targets.end()
        || pipeline_found == ins_->pipelines.end())
    {
        return;
    }

    tex_found->second.descriptor.imageView = target_found->second.view;
    tex_found->second.descriptor.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    if (swapchain_idx >= pipeline_found->second.descriptor_sets.size()) {
        return;
    }

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = pipeline_found->second.descriptor_sets[swapchain_idx];
    write.dstBinding = 3;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &tex_found->second.descriptor;
    vkUpdateDescriptorSets(ins_->get_device(), 1, &write, 0, nullptr);

    for (const auto& item : draw_items_) {
        if (item.pipeline_name.empty() || item.transparent) {
            continue;
        }
        const auto item_tex_name = item.pipeline_name + ":shadowMap";
        auto item_tex = ins_->textures.find(item_tex_name);
        auto item_pipeline = ins_->pipelines.find(item.pipeline_name);
        if (item_tex == ins_->textures.end() || item_pipeline == ins_->pipelines.end()) {
            continue;
        }
        item_tex->second.descriptor = tex_found->second.descriptor;
        if (swapchain_idx >= item_pipeline->second.descriptor_sets.size()) {
            continue;
        }
        write.dstSet = item_pipeline->second.descriptor_sets[swapchain_idx];
        write.pImageInfo = &item_tex->second.descriptor;
        vkUpdateDescriptorSets(ins_->get_device(), 1, &write, 0, nullptr);
    }
}

void ForwardRenderer::update(const RenderView& view) {
    if (!ins_) {
        return;
    }

    update_camera_aspect();
    update_lights_from_scene();
    update_global_uniforms(view.swapchain_image_idx);

    // Per-object transform/material/light are uploaded in draw_batch via
    // sync_draw_item_uniforms so shared fallback pipelines stay correct.
    (void)view.delta_seconds;
}

void ForwardRenderer::draw_batch(VkCommandBuffer cmd, const RenderView& view,
    const std::vector<const ForwardDrawItem*>& items, const char* fallback_pipeline)
{
    for (const auto* item : items) {
        if (!item) {
            continue;
        }

        const std::string& pipeline_name = item->pipeline_name.empty()
            ? fallback_pipeline
            : item->pipeline_name;

        auto mesh_found = ins_->meshes.find(item->mesh_name);
        if (mesh_found == ins_->meshes.end()) {
            continue;
        }

        auto pipeline_found = ins_->pipelines.find(pipeline_name);
        if (pipeline_found == ins_->pipelines.end()) {
            continue;
        }

        const auto& pipeline = pipeline_found->second;
        ins_->bind_graphics_pipeline(cmd, pipeline.pipeline);

        if (pipeline_name == kShadowPipeline) {
            ShadowTransformUBO shadow_transform{};
            shadow_transform.model = item->model;
            shadow_transform.lightView = glm::lookAt(
                glm::vec3(light_ubo_.lightPos),
                glm::vec3(0.0f),
                glm::vec3(0.0f, 1.0f, 0.0f));
            shadow_transform.lightProj = glm::ortho(-6.0f, 6.0f, -6.0f, 6.0f, 0.1f, 30.0f);
            const auto shadow_ubo_name = std::string(kShadowPipeline) + ":shadow";
            auto shadow_ubo_found = ins_->ubos.find(shadow_ubo_name);
            if (shadow_ubo_found != ins_->ubos.end()
                && view.swapchain_image_idx < shadow_ubo_found->second.memos.size())
            {
                ins_->sync_uniform(shadow_ubo_found->second.memos[view.swapchain_image_idx],
                    &shadow_transform, sizeof(shadow_transform));
            }
        }
        else {
            sync_draw_item_uniforms(view.swapchain_image_idx, *item, pipeline_name);
        }

        mesh_found->second.emit_draw_cmd(
            cmd,
            pipeline.ppl_layout,
            &pipeline.descriptor_sets[view.swapchain_image_idx]);
    }
}

void ForwardRenderer::pass_shadow_map(VkCommandBuffer cmd, const RenderView& view) {
    if (!shadow_pipeline_ready_ || !shadow_target_ready_ || shadow_framebuffer_ == VK_NULL_HANDLE) {
        return;
    }

    VkClearValue clear_value{};
    clear_value.depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo renderpass_info{};
    renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderpass_info.renderPass = shadow_render_pass_;
    renderpass_info.framebuffer = shadow_framebuffer_;
    renderpass_info.renderArea.offset = {0, 0};
    renderpass_info.renderArea.extent = {width_, height_};
    renderpass_info.clearValueCount = 1;
    renderpass_info.pClearValues = &clear_value;

    vkCmdBeginRenderPass(cmd, &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);

    std::vector<const ForwardDrawItem*> casters;
    for (const auto& item : draw_items_) {
        if (!item.transparent) {
            casters.push_back(&item);
        }
    }
    draw_batch(cmd, view, casters, kShadowPipeline);

    vkCmdEndRenderPass(cmd);

    if (shadow_target_ready_) {
        auto target_found = ins_->render_targets.find(kShadowMapTarget);
        if (target_found != ins_->render_targets.end()) {
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = target_found->second.image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.layerCount = 1;

            vkCmdPipelineBarrier(
                cmd,
                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier);
        }
    }

    refresh_shadow_map_descriptors(view.swapchain_image_idx);
}

void ForwardRenderer::pass_opaque(VkCommandBuffer cmd, const RenderView& view) {
    std::vector<const ForwardDrawItem*> opaque_items;
    opaque_items.reserve(draw_items_.size());
    for (const auto& item : draw_items_) {
        if (!item.transparent) {
            opaque_items.push_back(&item);
        }
    }
    draw_batch(cmd, view, opaque_items, kOpaquePipeline);
}

void ForwardRenderer::pass_transparent(VkCommandBuffer cmd, const RenderView& view) {
    std::vector<const ForwardDrawItem*> transparent_items;
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
        [this](const ForwardDrawItem* a, const ForwardDrawItem* b) {
            const glm::vec3 a_center = glm::vec3(a->model * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            const glm::vec3 b_center = glm::vec3(b->model * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            const glm::vec3 to_a = a_center - camera_->pos;
            const glm::vec3 to_b = b_center - camera_->pos;
            return glm::dot(to_a, to_a) > glm::dot(to_b, to_b);
        });

    draw_batch(cmd, view, transparent_items, kTransparentPipeline);
}

void ForwardRenderer::pass_post_process(VkCommandBuffer cmd, const RenderView& view) {
    if (!post_pipeline_ready_) {
        return;
    }

    auto pipeline_found = ins_->pipelines.find(kPostPipeline);
    if (pipeline_found == ins_->pipelines.end()) {
        return;
    }

    const auto post_ubo_name = std::string(kPostPipeline) + ":post";
    auto post_ubo_found = ins_->ubos.find(post_ubo_name);
    if (post_ubo_found != ins_->ubos.end()
        && view.swapchain_image_idx < post_ubo_found->second.memos.size())
    {
        ins_->sync_uniform(post_ubo_found->second.memos[view.swapchain_image_idx],
            &post_params_ubo_, sizeof(post_params_ubo_));
    }

    const auto& pipeline = pipeline_found->second;
    if (view.swapchain_image_idx >= pipeline.descriptor_sets.size()) {
        return;
    }

    ins_->bind_graphics_pipeline(cmd, pipeline.pipeline);
    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline.ppl_layout,
        0,
        1,
        &pipeline.descriptor_sets[view.swapchain_image_idx],
        0,
        nullptr);
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

void ForwardRenderer::record_commands(VkCommandBuffer cmd, const RenderView& view) {
    if (!ins_) {
        return;
    }

    pass_shadow_map(cmd, view);

    VkRenderPassBeginInfo renderpass_info{};
    renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderpass_info.renderPass = ins_->get_renderpass();
    renderpass_info.framebuffer = ins_->get_framebuffers()[view.swapchain_image_idx];
    renderpass_info.renderArea.offset = {0, 0};
    renderpass_info.renderArea.extent = ins_->get_swapchain_extent();

    std::array<VkClearValue, 2> clear_values{};
    clear_values[0].color = {{0.02f, 0.02f, 0.05f, 1.0f}};
    clear_values[1].depthStencil = {1.0f, 0};
    renderpass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
    renderpass_info.pClearValues = clear_values.data();

    vkCmdBeginRenderPass(cmd, &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);
    pass_opaque(cmd, view);
    pass_transparent(cmd, view);
    pass_post_process(cmd, view);
    if (overlay_draw_) {
        overlay_draw_(cmd);
    }
    vkCmdEndRenderPass(cmd);
}

} // namespace vkkk
