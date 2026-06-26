#include "renderer/forwardp.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace vkkk
{

// public funcs
bool ForwardPRenderer::initialize(Context* context) {
    ctx = context;
    if (!ctx) {
        return false;
    }

    if (ctx->get_window()) {
        int w = 0;
        int h = 0;
        glfwGetFramebufferSize(ctx->get_window(), &w, &h);
        width = static_cast<uint32_t>(std::max(w, 1));
    }

    return true;
}

void ForwardPRenderer::shutdown() {
    opaque_batch.clear();
    transparent_batch.clear();
    ctx = nullptr;
    scene = nullptr;
    camera = nullptr;
}

void ForwardPRenderer::on_resize(uint32_t width, uint32_t height) {
    this->width = width;
    this->height = height;
}

void ForwardPRenderer::update(const RenderView& view) {

}

void ForwardPRenderer::record_commands(vk::CommandBuffer cmd, const RenderView& view) {
    if (!ctx) {
        return;
    }

    pass_opaque(cmd, view);
    pass_transparent(cmd, view);
}

// private funcs

}