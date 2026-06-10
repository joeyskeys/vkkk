#include <stdexcept>
#include <vector>

#include "vk_ins/context.hpp"

namespace vkkk
{

std::vector<const char*> Context::get_glfw_instance_extensions() {
    uint32_t count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);
    if (extensions == nullptr || count == 0) {
        throw std::runtime_error("failed to query GLFW instance extensions");
    }
    return {extensions, extensions + count};
}

GLFWwindow* Context::create_window(int width, int height, const char* title, bool resizable) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, resizable ? GLFW_TRUE : GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (window == nullptr) {
        throw std::runtime_error("failed to create GLFW window");
    }
    return window;
}

void Context::init_glfw(int width, int height, const char* title, bool resizable) {
    GLFWwindow* window = create_window(width, height, title, resizable);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* win, int, int) {
        auto* ctx = static_cast<Context*>(glfwGetWindowUserPointer(win));
        if (ctx != nullptr) {
            ctx->frame_buffer_resized = true;
        }
    });
    init(window);
}

void Context::recreate_swapchain() {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window_, &width, &height);
        glfwWaitEvents();
    }
    device.waitIdle();

    swapchain_image_views.clear();
    depth_view = nullptr;
    depth_image = nullptr;
    depth_memo = nullptr;
    swapchain = nullptr;
    images_in_flight.assign(swapchain_images.size(), VK_NULL_HANDLE);

    create_swapchain();
    create_imageviews();
    create_depth_resources();

    vk::CommandBufferAllocateInfo cmd_buf_alloc_info{};
    cmd_buf_alloc_info.commandPool = command_pool;
    cmd_buf_alloc_info.level = vk::CommandBufferLevel::ePrimary;
    cmd_buf_alloc_info.commandBufferCount = static_cast<uint32_t>(swapchain_images.size());
    command_buffers = vk::raii::CommandBuffers(device, cmd_buf_alloc_info);
}

} // namespace vkkk
