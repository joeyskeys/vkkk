#pragma once

#include "renderer/renderer.h"

namespace vkkk
{

class DeferredRenderer final : public Renderer {
public:
    const char* type_name() const override { return "Deferred"; }

    bool initialize(Context* context) override {
        ctx = context;
        return ctx != nullptr;
    }

    void shutdown() override {}

    void update(const RenderView& view) override {
        (void)view;
    }

    void on_resize(uint32_t width, uint32_t height) override {
        (void)width;
        (void)height;
    }
};

} // namespace vkkk
