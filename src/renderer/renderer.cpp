#include "renderer/renderer.hpp"
#include <stdexcept>

#include "asset_mgr/scene.h"
#include "asset_mgr/light_mgr.h"
#include "concepts/camera.h"

namespace vkkk
{

void Renderer::update() {
    // update uniform attrs on CPU side
    scene->camera->update_ubo_data();
}

bool Renderer::create_pipeline_from_shader_src(const std::string& ppl_name,
    const char* vert,
    const char* frag,
    const PipelineOption& option,
    const std::vector<VERT_COMP>& components,
    bool interleaved,
    bool depth_only)
{
    if (!ctx) {
        return false;
    }

    ShaderModule vert_module, frag_module;
	vert_module.load(vert, vk::ShaderStageFlagBits::eVertex, "vert_shader");
	frag_module.load(frag, vk::ShaderStageFlagBits::eFragment, "frag_shader");
    ShaderModulePack pack;
    if (!(pack.add_shader_module(vert_module) && pack.add_shader_module(frag_module)))
    {
        return false;
    }
    return ctx->create_pipeline(ppl_name, pack, option, components, interleaved, depth_only);
}

bool Renderer::create_pipeline_from_shader_src(const std::string& ppl_name,
    const char* task,
    const char* mesh,
    const char* frag,
    const PipelineOption& option,
    const std::vector<VERT_COMP>& components)
{
    if (!ctx) {
        return false;
    }

    ShaderModule task_module, mesh_module, frag_module;
    if (!task_module.load(task, vk::ShaderStageFlagBits::eTaskEXT, "task_shader")
        || !mesh_module.load(mesh, vk::ShaderStageFlagBits::eMeshEXT, "mesh_shader")
        || !frag_module.load(frag, vk::ShaderStageFlagBits::eFragment, "frag_shader"))
    {
        return false;
    }

    ShaderModulePack pack;
    if (!(pack.add_shader_module(task_module)
        && pack.add_shader_module(mesh_module)
        && pack.add_shader_module(frag_module)))
    {
        return false;
    }
    return ctx->create_pipeline(ppl_name, pack, option, components);
}

void Renderer::sync_uniforms(const uint32_t swapchain_idx, const Scene* scene, const std::string& pipeline_name) {
    if (!ctx || !scene || !scene->camera || !scene->light_mgr) {
        return;
    }

    const auto pipeline_it = ctx->pipelines.find(pipeline_name);
    if (pipeline_it == ctx->pipelines.end()) {
        return;
    }

    const auto& ubos = pipeline_it->second.ubos;
    const auto* light_storage = scene->light_mgr->pipeline_storage(pipeline_name);

    if (ubos.contains(buf::CameraUBO)) {
        sync_uniform(buf::CameraUBO, swapchain_idx, &(scene->camera->ubo_data), pipeline_name);
    }
    if (light_storage != nullptr) {
        if (ubos.contains(buf::PointLightUBO) && !light_storage->pt_lights.empty()) {
            sync_uniform(buf::PointLightUBO, swapchain_idx, light_storage->pt_lights.data(), pipeline_name);
        }
        if (ubos.contains(buf::DirectionalLightUBO) && !light_storage->dir_lights.empty()) {
            sync_uniform(buf::DirectionalLightUBO, swapchain_idx, light_storage->dir_lights.data(),
                pipeline_name);
        }
        if (ubos.contains(buf::SpotLightUBO) && !light_storage->spot_lights.empty()) {
            sync_uniform(buf::SpotLightUBO, swapchain_idx, light_storage->spot_lights.data(),
                pipeline_name);
        }
    }
}

}
