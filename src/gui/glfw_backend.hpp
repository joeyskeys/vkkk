#pragma once

#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "gui/gui.h"
#include "gui/window_backend.hpp"

namespace vkkk
{

class Context;

class GlfwBackend : public WindowBackend {
public:
    GlfwBackend(int width, int height, const char* title, bool resizable = true);
    ~GlfwBackend() override;

    GlfwBackend(const GlfwBackend&) = delete;
    GlfwBackend& operator=(const GlfwBackend&) = delete;

    GLFWwindow* glfw_window() const { return win; }
    ImGuiHud& hud() { return imgui_hud; }

    std::vector<const char*> instance_extensions(bool enable_validation) const override;
    VkSurfaceKHR create_surface(VkInstance instance) override;
    VkExtent2D framebuffer_size() const override;
    VkExtent2D window_size() const override;
    void wait_until_visible() override;
    bool should_close() const override;
    void poll_events() override;
    void set_resize_flag(bool* resized) override;
    void* native_handle() const override;
    void cursor_position(double& x, double& y) const override;
    bool mouse_pressed(int button) const override;

private:
    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);

    GLFWwindow* win = nullptr;
    bool* resize_flag = nullptr;
    ImGuiHud imgui_hud;
};

GLFWwindow* glfw_window(const Context& ctx);

static_assert(WindowBackendType<GlfwBackend>);

} // namespace vkkk
