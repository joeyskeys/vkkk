#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif

#include "gui/glfw_backend.hpp"

#include <stdexcept>

#include "vk_ins/context.hpp"

#ifdef _WIN32
#include <GLFW/glfw3native.h>
#endif

namespace vkkk
{

namespace
{

int glfw_refcount = 0;

} // namespace

GlfwBackend::GlfwBackend(int width, int height, const char* title, bool resizable) {
    if (glfw_refcount == 0) {
        if (glfwInit() == GLFW_FALSE) {
            throw std::runtime_error("failed to initialize GLFW");
        }
    }
    ++glfw_refcount;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, resizable ? GLFW_TRUE : GLFW_FALSE);

    win = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (win == nullptr) {
        --glfw_refcount;
        if (glfw_refcount == 0) {
            glfwTerminate();
        }
        throw std::runtime_error("failed to create GLFW window");
    }

    glfwSetWindowUserPointer(win, this);
    glfwSetFramebufferSizeCallback(win, framebuffer_size_callback);
}

GlfwBackend::~GlfwBackend() {
    if (win != nullptr) {
        glfwDestroyWindow(win);
        win = nullptr;
    }
    if (glfw_refcount > 0) {
        --glfw_refcount;
        if (glfw_refcount == 0) {
            glfwTerminate();
        }
    }
}

void GlfwBackend::framebuffer_size_callback(GLFWwindow* window, int, int) {
    auto* backend = static_cast<GlfwBackend*>(glfwGetWindowUserPointer(window));
    if (backend != nullptr && backend->resize_flag != nullptr) {
        *backend->resize_flag = true;
    }
}

std::vector<const char*> GlfwBackend::instance_extensions(bool enable_validation) const {
    uint32_t count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&count);
    std::vector extensions(glfw_extensions, glfw_extensions + count);
    if (enable_validation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return extensions;
}

VkSurfaceKHR GlfwBackend::create_surface(VkInstance instance) {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(instance, win, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface");
    }
    return surface;
}

VkExtent2D GlfwBackend::framebuffer_size() const {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(win, &width, &height);
    return VkExtent2D{
        width < 0 ? 0u : static_cast<uint32_t>(width),
        height < 0 ? 0u : static_cast<uint32_t>(height)
    };
}

VkExtent2D GlfwBackend::window_size() const {
    int width = 0;
    int height = 0;
    glfwGetWindowSize(win, &width, &height);
    return VkExtent2D{
        width < 0 ? 0u : static_cast<uint32_t>(width),
        height < 0 ? 0u : static_cast<uint32_t>(height)
    };
}

void GlfwBackend::wait_until_visible() {
    VkExtent2D size = framebuffer_size();
    while (size.width == 0 || size.height == 0) {
        glfwWaitEvents();
        size = framebuffer_size();
    }
}

bool GlfwBackend::should_close() const {
    return glfwWindowShouldClose(win) == GLFW_TRUE;
}

void GlfwBackend::poll_events() {
    glfwPollEvents();
}

void GlfwBackend::set_resize_flag(bool* resized) {
    resize_flag = resized;
}

void* GlfwBackend::native_handle() const {
#ifdef _WIN32
    return glfwGetWin32Window(win);
#else
    return nullptr;
#endif
}

void GlfwBackend::cursor_position(double& x, double& y) const {
    glfwGetCursorPos(win, &x, &y);
}

bool GlfwBackend::mouse_pressed(int button) const {
    return glfwGetMouseButton(win, button) == GLFW_PRESS;
}

GLFWwindow* glfw_window(const Context& ctx) {
    const auto* backend = dynamic_cast<const GlfwBackend*>(ctx.window());
    return backend != nullptr ? backend->glfw_window() : nullptr;
}

} // namespace vkkk
