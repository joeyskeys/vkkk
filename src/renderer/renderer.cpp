#include "renderer/renderer.hpp"
#include <stdexcept>

namespace vkkk
{

bool Renderer::create_pipeline_from_shader_src(const string& ppl_name,
    const char* vert_or_mesh,
    const char* frag,
    const PipelineOption& option)
{
    if (!ctx) {
        return false;
    }
    ShaderModulePack pack;
    if (!(pack.add_shader_module(vert_or_mesh) && pack.add_shader_module(frag))) {
        return false;
    }
    return ctx->create_pipeline(ppl_name, pack, option, components);
}

void Renderer::sync_uniforms(const uint32_t swapchain_idx, const InstanceAttr& instance_attr, const std::string& pipeline_name) {
    if (!ctx) {
        return;
    }

    auto sync_if_present = [&](const std::string& ubo_suffix, const void* data, size_t size) {
        const auto ubo_name = pipeline_name + ubo_suffix;
        UBO* ubo = nullptr;
        try {
            ubo = &ctx->require_ubo(ubo_name);
        } catch (const std::runtime_error&) {
            return;
        }
        if (swapchain_idx >= ubo->memos.size()) {
            return;
        }
        ctx->sync_uniform(ubo->memos[swapchain_idx], data, static_cast<uint32_t>(size));
    };

    sync_if_present(":CameraUBO", instance_attr.get_data(), instance_attr.size());
    sync_if_present(":PointLightUBO", instance_attr.get_data(), instance_attr.size());
    sync_if_present(":DirectionalLightUBO", instance_attr.get_data(), instance_attr.size());
    sync_if_present(":SpotLightUBO", instance_attr.get_data(), instance_attr.size());
}

void Renderer::sync_ssbos(const uint32_t swapchain_idx, const InstanceAttr& instance_attr, const Pipeline& pipeline) {

}