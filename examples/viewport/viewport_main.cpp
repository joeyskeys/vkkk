#include <glm/geometric.hpp>

#include <GLFW/glfw3.h>

#include "concepts/camera.h"
#include "vp/frame_axis.hpp"
#include "vp/viewport.hpp"

namespace
{

constexpr uint32_t kWidth = 800;
constexpr uint32_t kHeight = 600;

} // namespace

int main() {
    vkkk::Context ctx;
    GLFWwindow* window = ctx.init_glfw(kWidth, kHeight, "vkkk Viewport", true);
    const auto glfw_extensions = vkkk::Context::get_glfw_instance_extensions();
    ctx.init(window, "vkkk", VK_MAKE_VERSION(1, 0, 0), "vulkan",
        vk::ApiVersion13, true, {}, glfw_extensions);

    vkkk::Camera camera{
        glm::vec3{3.0f, 3.0f, 3.0f},
        glm::normalize(glm::vec3{-1.0f, -1.0f, -1.0f}),
        glm::vec3{0.0f, 1.0f, 0.0f},
        45.0f,
        kWidth / static_cast<float>(kHeight),
        0.1f,
        100.0f,
    };
    camera.update_ubo_data();

    using BasicViewport = vkkk::vp::Viewport<vkkk::vp::FrameAxisFeature>;
    BasicViewport viewport(ctx);
    viewport.add_feature<vkkk::vp::FrameAxisFeature>(camera);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        vkkk::Context::Frame frame{};
        if (!viewport.begin_frame(frame)) {
            continue;
        }

        const auto extent = viewport.extent();
        camera.ratio = static_cast<float>(extent.width)
            / static_cast<float>(extent.height == 0 ? 1 : extent.height);
        camera.update_ubo_data();

        viewport.update(frame);
        viewport.record_frame(frame);
        viewport.end_frame(frame);
    }

    ctx.wait_idle();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
