#pragma once

#include <concepts>
#include <vector>

#include <vulkan/vulkan.h>

#include "gui/input.hpp"

namespace vkkk
{

// Toolkit-neutral window/surface owner used by Context for instance extensions,
// VkSurfaceKHR creation, swapchain sizing, and resize notification.
class WindowBackend {
public:
    virtual ~WindowBackend() = default;

    virtual std::vector<const char*> instance_extensions(bool enable_validation) const = 0;
    virtual VkSurfaceKHR create_surface(VkInstance instance) = 0;
    virtual VkExtent2D framebuffer_size() const = 0;
    virtual VkExtent2D window_size() const = 0;
    virtual void wait_until_visible() = 0;
    virtual bool should_close() const = 0;
    virtual void poll_events() = 0;
    virtual void set_resize_flag(bool* resized) = 0;
    virtual void* native_handle() const = 0;

    virtual InputPointer pointer() const = 0;
    virtual bool mouse_down(MouseButton button) const = 0;
    virtual bool key_down(Key key) const = 0;
    virtual uint32_t modifiers() const = 0;

    bool map_cursor(double x, double y, VkExtent2D target, uint32_t& px, uint32_t& py) const {
        InputPointer current = pointer();
        current.x = x;
        current.y = y;
        return current.to_pixel(target, px, py);
    }
};

template <typename T>
concept WindowBackendType = std::derived_from<T, WindowBackend>;

} // namespace vkkk
