#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include <vulkan/vulkan.hpp>

#include "built_in_shader/common.h"
#include "vk_ins/context.hpp"

namespace vkkk
{

class Scene;
class Context;

struct RenderView {
    uint32_t swapchain_image_idx = 0;
    float delta_seconds = 0.0f;
};

class Renderer {
public:
    Renderer(Context* context, Scene* scene) : ctx(context), scene(scene) {}
    virtual ~Renderer() = default;

    virtual const char* type_name() const = 0;

    // Bind renderer lifetime to an already initialized Vulkan instance.
    virtual bool initialize(Context* context) = 0;
    virtual void shutdown() = 0;

    // Scene data to be consumed by concrete renderer implementation.
    virtual void set_scene(Scene* s) { scene = s; }

    // Called once per frame before command buffer submission.
    virtual void update();

    // Record draw commands into the active swapchain command buffer.
    virtual void record_commands(vk::CommandBuffer cmd, const uint32_t swapchain_image_idx) = 0;

    // Called when swapchain extent or render targets change.
    virtual void on_resize(uint32_t width, uint32_t height) = 0;

    // utils
    bool create_pipeline_from_shader_src(const std::string& ppl_name,
        const char* vert_or_mesh,
        const char* frag,
        const PipelineOption& option,
        const std::vector<VERT_COMP>& components = {VERTEX});

    template <typename T>
    void sync_uniform(const UBOType type, const uint32_t swapchain_idx,
        const T* ubo_data,
        const Pipeline& pipeline)
    {
        if (!ctx) {
            return;
        }

        auto ubo_it = pipeline.ubos.find(type);
        if (ubo_it == pipeline.ubos.end()) {
            return;
        }
        auto* ubo = &ubo_it->second;
        if (swapchain_idx >= ubo->memos.size()) {
            return;
        }
        ctx->sync_uniform(ubo->memos[swapchain_idx], ubo_data, ubo->size * ubo->vecsize);
    }

    void sync_ssbo(const void* ssbo_data,
        const Pipeline& pipeline)
    {
        if (!ctx) {
            return;
        }

        auto ssbo_it = pipeline.ssbos.find(SSBOType_InstanceAttrs);
        if (ssbo_it == pipeline.ssbos.end()) {
            return;
        }
        ctx->sync_ssbo(ssbo_it->second, ssbo_data);
    }

    void sync_uniforms(const uint32_t swapchain_idx, const Scene* scene, const std::string pipeline_name, const Pipeline& pipeline);

public:
    Context* ctx = nullptr;
    Scene* scene = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;

    // intermediate ssbo data: pipeline name, mesh name, ssbo buffer
    using IntermediateSSBOData = std::unordered_map<std::string, std::pair<size_t, std::unordered_map<std::string, std::pair<size_t, std::vector<char>>>>>;
    // batch: value: pipeline name, key: ssbo buffer, offset vector
    using Batch = std::unordered_map<std::string, std::pair<std::unique_ptr<char[]>, std::vector<std::tuple<std::string, size_t, size_t>>>>;
};

} // namespace vkkk
