#pragma once

#include <concepts>
#include <vector>

#include <vulkan/vulkan.h>

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
    virtual void cursor_position(double& x, double& y) const = 0;
    // 0 = left, 1 = right, 2 = middle
    virtual bool mouse_pressed(int button) const = 0;
};

template <typename T>
concept WindowBackendType = std::derived_from<T, WindowBackend>;

} // namespace vkkk
