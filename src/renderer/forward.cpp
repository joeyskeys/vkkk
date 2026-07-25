#include "renderer/forward.h"

#include <algorithm>
#include <cstring>

#include <glm/gtc/matrix_transform.hpp>

#include "asset_mgr/light_mgr.h"
#include "asset_mgr/scene.h"
#include "concepts/camera.h"

namespace vkkk
{

bool ForwardRenderer::initialize(Context* context) {
    ctx = context;
    if (ctx == nullptr) {
        return false;
    }

    if (ctx->get_window()) {
        int framebuffer_width = 0;
        int framebuffer_height = 0;
        glfwGetFramebufferSize(ctx->get_window(), &framebuffer_width, &framebuffer_height);
        width = static_cast<uint32_t>(std::max(framebuffer_width, 1));
        height = static_cast<uint32_t>(std::max(framebuffer_height, 1));
    }

    shadowResolve.shadow_map_size_bias = glm::vec4(
        static_cast<float>(kShadowMapSize),
        static_cast<float>(kShadowMapSize),
        0.0005f,
        0.0f);
    shadowResolve.pcf_radius_reserved = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);

    if (!ctx->add_depth_attachment(kShadowMapSize, kShadowMapSize)) {
        return false;
    }
    shadowDepthAttachmentIndex = static_cast<uint32_t>(ctx->depth_attachments.size() - 1);
    return create_pass_pipelines();
}

void ForwardRenderer::shutdown() {
    opaqueIntermediateSsboDataMap.clear();
    transparentIntermediateSsboDataMap.clear();
    opaqueBatches.clear();
    transparentBatches.clear();
    shadowInstanceData.clear();
    shadowBatchInfos.clear();
    ctx = nullptr;
    scene = nullptr;
    shadowDepthAttachmentIndex = ~0u;
}

void ForwardRenderer::on_resize(uint32_t new_width, uint32_t new_height) {
    width = new_width;
    height = new_height;
}

PipelineOption ForwardRenderer::make_shadow_pipeline_option() const {
    PipelineOption option;
    option.setup_multisampling(false, vk::SampleCountFlagBits::e1);
    option.setup_rasterizer(false, false, vk::PolygonMode::eFill, 1.0f,
        vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise, false);
    option.setup_depth_stencil(true, true, vk::CompareOp::eLess, false, false);
    option.setup_viewport(0.0f, 0.0f, static_cast<float>(kShadowMapSize),
        static_cast<float>(kShadowMapSize), 0.0f, 1.0f);
    option.setup_scissor(0, 0, kShadowMapSize, kShadowMapSize);
    return option;
}

bool ForwardRenderer::create_pass_pipelines() {
    auto shadow_option = make_shadow_pipeline_option();
    return create_pipeline_from_shader_src(
        kShadowDepthPipeline, shadow_map_vert, shadow_map_frag, shadow_option,
        {VERTEX, NORMAL}, true, true);
}

void ForwardRenderer::allocate_ssbo() {
    if (ctx == nullptr) {
        return;
    }

    const auto build_batches = [](const IntermediateSSBODataMap& pending, Batches& batches) {
        for (const auto& [pipeline_name, pipeline_data] : pending) {
            size_t instance_offset = 0;
            size_t byte_offset = 0;
            auto& batch = batches[pipeline_name];
            batch.buffer = std::make_unique<char[]>(pipeline_data.total_size);
            batch.batch_infos.clear();
            for (const auto& [mesh_name, mesh_data] : pipeline_data.data_map) {
                std::memcpy(batch.buffer.get() + byte_offset, mesh_data.data.data(), mesh_data.data.size());
                batch.batch_infos.push_back(BatchInfo{
                    mesh_name,
                    mesh_data.data.size(),
                    mesh_data.instance_count,
                    instance_offset,
                });
                instance_offset += mesh_data.instance_count;
                byte_offset += mesh_data.data.size();
            }
        }
    };

    opaqueBatches.clear();
    transparentBatches.clear();
    build_batches(opaqueIntermediateSsboDataMap, opaqueBatches);
    build_batches(transparentIntermediateSsboDataMap, transparentBatches);

    shadowInstanceData.clear();
    shadowBatchInfos.clear();
    size_t shadow_instance_offset = 0;
    for (const auto& [pipeline_name, batch] : opaqueBatches) {
        (void)pipeline_name;
        if (batch.buffer == nullptr || batch.batch_infos.empty()) {
            continue;
        }

        size_t packed_bytes = 0;
        size_t instance_count = 0;
        for (const auto& info : batch.batch_infos) {
            packed_bytes += info.batch_total_size;
            instance_count += info.instance_count;
        }
        if (packed_bytes == 0 || instance_count == 0) {
            continue;
        }

        const size_t old_size = shadowInstanceData.size();
        shadowInstanceData.resize(old_size + packed_bytes);
        std::memcpy(shadowInstanceData.data() + old_size, batch.buffer.get(), packed_bytes);
        for (const auto& batch_info : batch.batch_infos) {
            auto shadow_info = batch_info;
            shadow_info.instance_offset += shadow_instance_offset;
            shadowBatchInfos.push_back(std::move(shadow_info));
        }
        shadow_instance_offset += instance_count;
    }

    const auto alloc_instance_ssbo = [&](const Batches& batches) {
        for (const auto& [pipeline_name, batch] : batches) {
            if (batch.batch_infos.empty()) {
                continue;
            }
            const size_t instance_count =
                batch.batch_infos.back().instance_offset + batch.batch_infos.back().instance_count;
            ctx->resize_pipeline_ssbo(pipeline_name, SSBOType_InstanceAttrs, std::max(instance_count, size_t{1}));
            ctx->alloc_pipeline_ssbo(pipeline_name, SSBOType_InstanceAttrs);
        }
    };
    alloc_instance_ssbo(opaqueBatches);
    alloc_instance_ssbo(transparentBatches);

    const size_t shadow_instance_count = shadowBatchInfos.empty() ? 1
        : shadowBatchInfos.back().instance_offset + shadowBatchInfos.back().instance_count;
    ctx->resize_pipeline_ssbo(kShadowDepthPipeline, SSBOType_InstanceAttrs, shadow_instance_count);
    ctx->alloc_pipeline_ssbo(kShadowDepthPipeline, SSBOType_InstanceAttrs);
}

void ForwardRenderer::update_main_directional_shadow() {
    // Fallback: enter through open front (+Z), 45° down into the box.
    mainDirectionalShadow.direction = glm::vec4(glm::normalize(glm::vec3(0.0f, -1.0f, -1.0f)), 0.0f);
    if (scene != nullptr && scene->light_mgr != nullptr) {
        for (const auto& [pipeline_name, _] : opaqueBatches) {
            const auto* storage = scene->light_mgr->pipeline_storage(pipeline_name);
            if (storage != nullptr && !storage->dir_lights.empty()) {
                mainDirectionalShadow.direction = storage->dir_lights.front().vec;
                mainDirectionalShadow.direction.w = 0.0f;
                break;
            }
        }
    }

    const glm::vec3 direction = glm::normalize(glm::vec3(mainDirectionalShadow.direction));
    // Place the light outside the cornell box looking at the origin.
    const glm::vec3 light_position = -direction * 5.0f;
    const glm::vec3 up = std::abs(direction.y) > 0.99f
        ? glm::vec3(0.0f, 0.0f, 1.0f)
        : glm::vec3(0.0f, 1.0f, 0.0f);
    // Explicit RH_ZO ortho: Vulkan clip Z is [0,1]. Fit the cornell room (~[-1,1]^3).
    glm::mat4 projection = glm::orthoRH_ZO(-2.5f, 2.5f, -2.5f, 2.5f, 0.5f, 12.0f);
    projection[1][1] *= -1.0f;
    mainDirectionalShadow.light_view_proj =
        projection * glm::lookAtRH(light_position, glm::vec3(0.0f), up);

    if (scene != nullptr && scene->camera != nullptr) {
        shadowResolve.inv_view_proj = glm::inverse(
            scene->camera->ubo_data.proj * scene->camera->ubo_data.view);
    }
}

void ForwardRenderer::sync_shadow_resources(uint32_t swapchain_image_idx) {
    if (ctx == nullptr) {
        return;
    }

    const auto sync_pipeline_shadow = [&](const std::string& pipeline_name) {
        ctx->sync_ubo(pipeline_name, UBOType_MainDirectionalShadow, &mainDirectionalShadow,
            swapchain_image_idx, static_cast<uint32_t>(sizeof(mainDirectionalShadow)));
        ctx->sync_ssbo(pipeline_name, SSBOType_MainDirectionalShadow, &mainDirectionalShadow,
            swapchain_image_idx, static_cast<uint32_t>(sizeof(mainDirectionalShadow)));
        ctx->sync_ubo(pipeline_name, UBOType_ShadowResolve, &shadowResolve,
            swapchain_image_idx, static_cast<uint32_t>(sizeof(shadowResolve)));
    };

    sync_pipeline_shadow(kShadowDepthPipeline);
    for (const auto& [pipeline_name, _] : opaqueBatches) {
        sync_pipeline_shadow(pipeline_name);
    }

    if (!shadowInstanceData.empty()) {
        ctx->sync_ssbo(kShadowDepthPipeline, SSBOType_InstanceAttrs, shadowInstanceData.data(),
            swapchain_image_idx, static_cast<uint32_t>(shadowInstanceData.size()));
    }
}

void ForwardRenderer::pass_shadow(vk::raii::CommandBuffer& cmd, uint32_t swapchain_image_idx) {
    if (ctx == nullptr || shadowDepthAttachmentIndex >= ctx->depth_attachments.size()
        || shadowBatchInfos.empty())
    {
        return;
    }

    update_main_directional_shadow();
    sync_shadow_resources(swapchain_image_idx);

    ctx->record_depth_pass(cmd, shadowDepthAttachmentIndex, [&](vk::raii::CommandBuffer& pass_cmd) {
        if (!ctx->bind(pass_cmd, kShadowDepthPipeline, swapchain_image_idx)) {
            return;
        }
        for (const auto& batch_info : shadowBatchInfos) {
            ctx->draw(pass_cmd, kShadowDepthPipeline, batch_info.mesh_name,
                static_cast<uint32_t>(batch_info.instance_count),
                static_cast<uint32_t>(batch_info.instance_offset));
        }
    });
}

void ForwardRenderer::draw_batch(vk::CommandBuffer cmd, uint32_t swapchain_image_idx,
    const Batches& batches)
{
    if (ctx == nullptr) {
        return;
    }

    for (const auto& [pipeline_name, batch] : batches) {
        if (batch.buffer == nullptr || !ctx->pipelines.contains(pipeline_name)) {
            continue;
        }

        sync_uniforms(swapchain_image_idx, scene, pipeline_name);

        const auto& pipeline = ctx->pipelines.at(pipeline_name);
        const auto ssbo_it = pipeline.ssbos.find(SSBOType_InstanceAttrs);
        const size_t elem_size = ssbo_it != pipeline.ssbos.end() ? ssbo_it->second.size : 0;
        size_t upload_bytes = 0;
        for (const auto& batch_info : batch.batch_infos) {
            upload_bytes = std::max(upload_bytes,
                (batch_info.instance_offset + batch_info.instance_count) * elem_size);
        }
        ctx->sync_ssbo(pipeline_name, SSBOType_InstanceAttrs, batch.buffer.get(),
            swapchain_image_idx, static_cast<uint32_t>(upload_bytes));

        if (!ctx->bind(cmd, pipeline_name, swapchain_image_idx)) {
            continue;
        }
        for (const auto& batch_info : batch.batch_infos) {
            ctx->draw(cmd, pipeline_name, batch_info.mesh_name,
                static_cast<uint32_t>(batch_info.instance_count),
                static_cast<uint32_t>(batch_info.instance_offset));
        }
    }
}

void ForwardRenderer::pass_opaque(vk::CommandBuffer cmd, uint32_t swapchain_image_idx) {
    draw_batch(cmd, swapchain_image_idx, opaqueBatches);
}

void ForwardRenderer::pass_transparent(vk::CommandBuffer cmd, uint32_t swapchain_image_idx) {
    draw_batch(cmd, swapchain_image_idx, transparentBatches);
}

void ForwardRenderer::record_commands(vk::CommandBuffer cmd, uint32_t swapchain_image_idx) {
    if (ctx == nullptr) {
        return;
    }
    pass_opaque(cmd, swapchain_image_idx);
    pass_transparent(cmd, swapchain_image_idx);
    if (overlayDraw) {
        overlayDraw(cmd);
    }
}

} // namespace vkkk
