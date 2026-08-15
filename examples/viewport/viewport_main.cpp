#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

#include <GLFW/glfw3.h>

#include "concepts/camera.h"
#include "font/font.hpp"
#include "vp/frame_axis.hpp"
#include "vp/grid.hpp"
#include "vp/viewport.hpp"

namespace
{

constexpr uint32_t kWidth = 1200;
constexpr uint32_t kHeight = 800;

class BillboardTextFeature final
    : public vkkk::vp::ViewportFeature<vkkk::vp::ViewportPhase::Scene> {
public:
    BillboardTextFeature(const vkkk::Camera& scene_camera, std::filesystem::path path)
        : camera(scene_camera)
        , font_path(std::move(path))
    {
    }

    void on_attach(vkkk::Context& context, vk::Extent2D) {
        if (font_path.empty()) {
            return;
        }

        constexpr const char* billboard_name = "vp_example_label";
        vkkk::font::TextRenderer renderer(font_path);
        vkkk::font::TextRenderOptions text_options{};
        text_options.pixel_height = 64;
        text_options.color = glm::vec4{0.05f, 0.05f, 0.05f, 1.0f};
        const auto text_texture = renderer.render(context, "vkkk", text_options);
        if (!text_texture.valid()) {
            return;
        }

        vkkk::BillboardTextOptions options{};
        options.position = glm::vec3{0.0f, 1.0f, 0.0f};
        options.size = glm::vec2{1.5f,
            1.5f * static_cast<float>(text_texture.extent.height) / text_texture.extent.width};
        options.depth_test = false;
        ready = context.add_billboard_text(
            billboard_name, vkkk::BillboardTextSource::render_target(text_texture.target_index), options);
    }

    void on_record(vkkk::Context& context, vk::raii::CommandBuffer& cmd, uint32_t image_index) {
        if (ready) {
            context.draw_billboard_text(cmd, "vp_example_label", camera.ubo_data, image_index);
        }
    }

private:
    const vkkk::Camera& camera;
    std::filesystem::path font_path;
    bool ready = false;
};

class ViewportControls {
public:
    explicit ViewportControls(vkkk::Camera& camera)
        : camera(camera)
    {
    }

    void update(GLFWwindow* window) {
        const bool middle_down = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
        if (!middle_down) {
            dragging = false;
        }
        else {
            double cursor_x = 0.0;
            double cursor_y = 0.0;
            glfwGetCursorPos(window, &cursor_x, &cursor_y);
            if (!dragging) {
                previous_x = cursor_x;
                previous_y = cursor_y;
                dragging = true;
            }
            else {
                const float delta_x = static_cast<float>(cursor_x - previous_x);
                const float delta_y = static_cast<float>(cursor_y - previous_y);
                previous_x = cursor_x;
                previous_y = cursor_y;

                const bool shift_down = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
                    || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
                if (shift_down) {
                    pan(delta_x, delta_y);
                }
                else {
                    rotate(delta_x, delta_y);
                }
            }
        }

        if (scroll_delta != 0.0f) {
            zoom(scroll_delta);
            scroll_delta = 0.0f;
        }
    }

    void add_scroll(float amount) {
        scroll_delta += amount;
    }

private:
    void rotate(float delta_x, float delta_y) {
        const glm::vec3 world_up{0.0f, 1.0f, 0.0f};
        const glm::vec3 offset = camera.pos - target;
        const glm::vec3 right = glm::normalize(glm::cross(camera.front, world_up));
        const glm::quat yaw = glm::angleAxis(-delta_x * 0.005f, world_up);
        const glm::quat pitch = glm::angleAxis(-delta_y * 0.005f, right);

        camera.pos = target + pitch * yaw * offset;
        camera.front = glm::normalize(target - camera.pos);
        camera.up = world_up;
    }

    void pan(float delta_x, float delta_y) {
        const float distance = glm::length(camera.pos - target);
        const glm::vec3 right = glm::normalize(glm::cross(camera.front, camera.up));
        const glm::vec3 up = glm::normalize(glm::cross(right, camera.front));
        const glm::vec3 translation = (-right * delta_x + up * delta_y) * distance * 0.002f;
        camera.pos += translation;
        target += translation;
    }

    void zoom(float amount) {
        const glm::vec3 offset = camera.pos - target;
        const float distance = glm::length(offset);
        const float new_distance = std::max(0.1f, distance * (1.0f - amount * 0.1f));
        camera.pos = target + glm::normalize(offset) * new_distance;
        camera.front = glm::normalize(target - camera.pos);
    }

    vkkk::Camera& camera;
    glm::vec3 target{0.0f};
    bool dragging = false;
    double previous_x = 0.0;
    double previous_y = 0.0;
    float scroll_delta = 0.0f;
};

ViewportControls* controls = nullptr;

void scroll_callback(GLFWwindow*, double, double yoffset) {
    if (controls != nullptr) {
        controls->add_scroll(static_cast<float>(yoffset));
    }
}

} // namespace

int main(int argc, char** argv) {
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
    ViewportControls viewport_controls(camera);
    controls = &viewport_controls;
    glfwSetScrollCallback(window, scroll_callback);

    using BasicViewport = vkkk::vp::Viewport<
        vkkk::vp::GridFeature,
        vkkk::vp::FrameAxisFeature,
        BillboardTextFeature>;
    BasicViewport viewport(ctx);
    const std::filesystem::path bundled_font_path =
        std::filesystem::path{VKKK_SOURCE_DIR} / "resource/font/Roboto-Light.ttf";
    // An explicit path can still override the bundled font.
    const std::filesystem::path font_path =
        argc > 1 ? std::filesystem::path{argv[1]} : bundled_font_path;
    viewport.add_feature<vkkk::vp::GridFeature>(camera);
    viewport.add_feature<vkkk::vp::FrameAxisFeature>(camera, font_path);
    viewport.add_feature<BillboardTextFeature>(camera, font_path);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        vkkk::Context::Frame frame{};
        if (!viewport.begin_frame(frame)) {
            continue;
        }

        viewport_controls.update(window);
        const auto extent = viewport.extent();
        camera.ratio = static_cast<float>(extent.width)
            / static_cast<float>(extent.height == 0 ? 1 : extent.height);
        camera.update_ubo_data();

        viewport.update(frame);
        viewport.record_frame(frame);
        viewport.end_frame(frame);
    }

    ctx.wait_idle();
    controls = nullptr;
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
