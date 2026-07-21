#pragma once

#include <functional>
#include <string>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "built_in_shader/phong.h"
#include "built_in_shader/shadow.h"
#include "renderer/renderer.hpp"
#include "vk_ins/context.hpp"
#include "vk_ins/shader_module_pack.hpp"

namespace vkkk
{

class ForwardRenderer final : public Renderer {
public:
    static constexpr const char* kOpaquePipeline = "forward_opaque";
    static constexpr const char* kTransparentPipeline = "forward_transparent";
    static constexpr const char* kShadowDepthPipeline = "forward_shadow_depth";
    static constexpr uint32_t kShadowMapSize = 2048;

    ForwardRenderer(Context* context, Scene* scene) : Renderer(context, scene) {}

    const char* type_name() const override { return "Forward"; }

    bool initialize(Context* context) override;
    void shutdown() override;
    void on_resize(uint32_t width, uint32_t height) override;

    // Call from Context::record_cmds pre_render_func before the color pass.
    void pass_shadow(vk::raii::CommandBuffer& cmd, uint32_t swapchain_image_idx);
    void record_commands(vk::CommandBuffer cmd, uint32_t swapchain_image_idx) override;

    template <bool transparent>
    void add_drawable(const std::string& mesh_name, const std::string& pipeline_name,
        size_t instance_attr_size, const char* data);
    void add_opaque_drawable(const std::string& mesh_name, const std::string& pipeline_name,
        size_t instance_attr_size, const char* data) {
        add_drawable<false>(mesh_name, pipeline_name, instance_attr_size, data);
    }
    void add_transparent_drawable(const std::string& mesh_name, const std::string& pipeline_name,
        size_t instance_attr_size, const char* data) {
        add_drawable<true>(mesh_name, pipeline_name, instance_attr_size, data);
    }
    void allocate_ssbo();
    uint32_t shadow_depth_attachment_index() const { return shadowDepthAttachmentIndex; }

    void set_overlay_draw(std::function<void(vk::CommandBuffer)> draw) {
        overlayDraw = std::move(draw);
    }

private:
    bool create_pass_pipelines();
    void update_main_directional_shadow();
    void sync_shadow_resources(uint32_t swapchain_image_idx);
    void pass_opaque(vk::CommandBuffer cmd, uint32_t swapchain_image_idx);
    void pass_transparent(vk::CommandBuffer cmd, uint32_t swapchain_image_idx);
    void draw_batch(vk::CommandBuffer cmd, uint32_t swapchain_image_idx, const Batches& batches);

    PipelineOption make_shadow_pipeline_option() const;

    IntermediateSSBODataMap opaqueIntermediateSsboDataMap;
    IntermediateSSBODataMap transparentIntermediateSsboDataMap;
    Batches opaqueBatches;
    Batches transparentBatches;
    std::vector<char> shadowInstanceData;
    std::vector<BatchInfo> shadowBatchInfos;
    MainDirectionalShadowData mainDirectionalShadow{};
    ShadowResolveUBO shadowResolve{};
    uint32_t shadowDepthAttachmentIndex = ~0u;
    std::function<void(vk::CommandBuffer)> overlayDraw;
};

template <bool transparent>
void ForwardRenderer::add_drawable(const std::string& mesh_name, const std::string& pipeline_name,
    size_t instance_attr_size, const char* data)
{
    if (data == nullptr || instance_attr_size == 0 || pipeline_name.empty()) {
        return;
    }
    auto& pending = transparent ? transparentIntermediateSsboDataMap : opaqueIntermediateSsboDataMap;
    auto& entry = pending[pipeline_name].data_map[mesh_name];
    ++entry.instance_count;
    entry.data.insert(entry.data.end(), data, data + instance_attr_size);
    pending[pipeline_name].total_size += instance_attr_size;
}

} // namespace vkkk
