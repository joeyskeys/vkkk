#include "vp/viewport.hpp"

namespace vkkk::vp
{

Viewport::Viewport(Context& context)
    : context_(context)
    , extent_(context.extent())
{
}

bool Viewport::begin_frame(Context::Frame& frame) {
    if (!context_.begin_frame(frame)) {
        return false;
    }

    const auto current_extent = context_.extent();
    if (current_extent != extent_) {
        extent_ = current_extent;
        on_resize(extent_.width, extent_.height);
    }
    return true;
}

void Viewport::update(const Context::Frame& frame) {
    on_update(frame);
}

void Viewport::record_frame(const Context::Frame& frame) {
    context_.record_frame(frame, [this](vk::raii::CommandBuffer& cmd, uint32_t image_index) {
        on_record(cmd, image_index);
    });
}

void Viewport::end_frame(const Context::Frame& frame) {
    context_.end_frame(frame);
}

} // namespace vkkk::vp
