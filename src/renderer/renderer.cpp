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

}
