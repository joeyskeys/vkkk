#pragma once

#include <type_traits>
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
    InputPointer pointer() const override;
    bool mouse_down(MouseButton button) const override;
    bool key_down(Key key) const override;
    uint32_t modifiers() const override;

private:
    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);

    GLFWwindow* win = nullptr;
    bool* resize_flag = nullptr;
    ImGuiHud imgui_hud;
};

GLFWwindow* glfw_window(const Context& ctx);

Key key_from_glfw(int glfw_key);
int glfw_from_key(Key key);
MouseButton mouse_from_glfw(int glfw_button);
int glfw_from_mouse(MouseButton button);
InputAction action_from_glfw(int glfw_action);
uint32_t mods_from_glfw(int glfw_mods);

static_assert(WindowBackendType<GlfwBackend>);
static_assert(!std::is_abstract_v<GlfwBackend>);

} // namespace vkkk
