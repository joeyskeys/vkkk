#pragma once

#include "renderer/renderer.h"

namespace vkkk
{

class ForwardRenderer final : public Renderer {
public:
    const char* type_name() const override { return "Forward"; }

    bool initialize(VkWrappedInstance* instance) override {
        ins_ = instance;
        return ins_ != nullptr;
    }

    void shutdown() override {}

    void set_scene(Scene* scene) override {
        scene_ = scene;
    }

    void render(const RenderView& view) override {
        (void)view;
        // Forward path placeholder: main pass shading directly to color target.
    }

    void on_resize(uint32_t width, uint32_t height) override {
        (void)width;
        (void)height;
    }

private:
    VkWrappedInstance* ins_ = nullptr;
    Scene* scene_ = nullptr;
};

} // namespace vkkk
