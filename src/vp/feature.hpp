#pragma once

#include <cstdint>

#include "vk_ins/context.hpp"

namespace vkkk::vp
{

enum class ViewportPhase {
    Scene,
    ScreenOverlay,
};

// Static feature base. Derived classes inherit a phase and selectively replace
// these no-op lifecycle methods without any virtual dispatch.
template <ViewportPhase PhaseValue>
class ViewportFeature {
public:
    static constexpr ViewportPhase phase = PhaseValue;

    void on_attach(Context&, vk::Extent2D) {}
    void on_resize(Context&, vk::Extent2D) {}
    void on_update(Context&, const Context::Frame&) {}
    void on_record(Context&, vk::raii::CommandBuffer&, uint32_t) {}
};

} // namespace vkkk::vp
