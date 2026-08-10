#pragma once

#include <cstdint>

#include "vk_ins/context.hpp"

namespace vkkk::vp
{

// Non-owning viewport orchestration layer over an initialized Context.
// Per frame call: begin_frame -> update -> record_frame -> end_frame.
class Viewport {
public:
    explicit Viewport(Context& context);
    virtual ~Viewport() = default;

    bool begin_frame(Context::Frame& frame);
    void update(const Context::Frame& frame);
    void record_frame(const Context::Frame& frame);
    void end_frame(const Context::Frame& frame);

    vk::Extent2D extent() const { return extent_; }
    Context& context() { return context_; }
    const Context& context() const { return context_; }

protected:
    virtual void on_resize(uint32_t width, uint32_t height) {}
    virtual void on_update(const Context::Frame& frame) {}
    virtual void on_record(vk::raii::CommandBuffer& cmd, uint32_t image_index) {}

private:
    Context& context_;
    vk::Extent2D extent_{};
};

} // namespace vkkk::vp
