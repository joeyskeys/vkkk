#pragma once

#include "renderer/renderer.h"

namespace vkkk
{

class DeferredRenderer final : public Renderer {
public:
    const char* type_name() const override { return "Deferred"; }

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
        // Deferred path placeholder:
        // 1) Geometry pass to G-buffer
        // 2) Lighting pass to final target
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
