#include "renderer/renderer.hpp"
#include <stdexcept>

namespace vkkk
{

void Renderer::update() {
    // update uniform attrs on CPU side
    camera.update_ubo_data();
}

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

void Renderer::sync_uniforms(const uint32_t swapchain_idx, const Scene* scene, const Pipeline& pipeline) {
    if (!ctx) {
        return;
    }

    auto light_storage = scene->light_mgr.pipeline_storage(pipeline.name);
    
    for (const auto& [ubo_type, ubo] : pipeline.ubos) {
        if (swapchain_idx >= ubo.memos.size()) {
            continue;
        }

        switch (ubo_type) {
            case UBOType_Camera:
                sync_uniform(swapchain_idx, &(scene->camera.ubo_data), pipeline);
                break;
            case UBOType_PointLight:
                sync_uniform(swapchain_idx, light_storage->pt_lights.data(), pipeline);
                break;
            case UBOType_DirectionalLight:
                sync_uniform(swapchain_idx, light_storage->dir_lights.data(), pipeline);
                break;
            case UBOType_SpotLight:
                sync_uniform(swapchain_idx, light_storage->spot_lights.data(), pipeline);
                break;
            default:
                break;
        }
    }
}

}
