#include <chrono>
#include <cmath>
#include <iterator>
#include <stdexcept>
#include <thread>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include "asset_mgr/drawable_mgr.h"
#include "asset_mgr/light_mgr.h"
#include "asset_mgr/scene.h"
#include "built_in_shader/phong_plus.h"
#include "concepts/camera.h"
#include "gui/gui.h"
#include "renderer/forwardp.hpp"
#include "vk_ins/context.hpp"

namespace
{

constexpr uint32_t width = 800;
constexpr uint32_t height = 600;
constexpr char pipeline_name[] = "phong_plus_mat";

vkkk::Camera camera{
    glm::vec3{0.0f, 0.0f, 3.8f},
    glm::vec3{0.0f, 0.0f, -1.0f},
    glm::vec3{0.0f, 1.0f, 0.0f},
    35.0f,
    width / static_cast<float>(height),
    0.1f,
    100.0f
};

void key_callback(GLFWwindow* /*win*/, int key, int /*code*/, int action, int /*mods*/) {
    const bool key_down = action == GLFW_PRESS || action == GLFW_REPEAT;
    const bool key_up = action == GLFW_RELEASE;
    if (key == GLFW_KEY_E) camera.y = key_down ? 0.01f : (key_up ? 0.0f : camera.y);
    else if (key == GLFW_KEY_Q) camera.y = key_down ? -0.01f : (key_up ? 0.0f : camera.y);
    else if (key == GLFW_KEY_W) camera.z = key_down ? 0.01f : (key_up ? 0.0f : camera.z);
    else if (key == GLFW_KEY_S) camera.z = key_down ? -0.01f : (key_up ? 0.0f : camera.z);
    else if (key == GLFW_KEY_A) camera.x = key_down ? -0.01f : (key_up ? 0.0f : camera.x);
    else if (key == GLFW_KEY_D) camera.x = key_down ? 0.01f : (key_up ? 0.0f : camera.x);
}

void mouse_btn_callback(GLFWwindow* win, int btn, int action, int /*mods*/) {
    if (btn == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            camera.rotating = true;
            glfwGetCursorPos(win, &camera.prev_x, &camera.prev_y);
        } else {
            camera.rotating = false;
        }
    }
}

void mouse_pos_callback(GLFWwindow* /*win*/, double x, double y) {
    if (!camera.rotating) return;
    const float delta_x = static_cast<float>((x - camera.prev_x) / 100.0);
    const float delta_y = static_cast<float>((y - camera.prev_y) / 100.0);
    camera.prev_x = x;
    camera.prev_y = y;
    camera.rotation = glm::angleAxis(delta_x, glm::vec3(0.0f, 1.0f, 0.0f));
    camera.rotation = glm::angleAxis(-delta_y, glm::vec3(1.0f, 0.0f, 0.0f)) * camera.rotation;
}

vkkk::PipelineOption make_pipeline_option() {
    vkkk::PipelineOption option;
    option.setup_multisampling(false, vk::SampleCountFlagBits::e1);
    option.setup_rasterizer(false, false, vk::PolygonMode::eFill, 1.0f,
        vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise, false);
    option.setup_depth_stencil(true, true, vk::CompareOp::eLess, false, false);
    option.setup_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);
    option.setup_scissor(0, 0, width, height);
    return option;
}

vkkk::PhongPlusInstanceAttrs make_instance(
    const glm::mat4& model, const glm::vec3& color, float shininess)
{
    vkkk::PhongPlusInstanceAttrs attrs{};
    attrs.model = model;
    attrs.ambient = glm::vec4(color * 0.08f, 1.0f);
    attrs.diffuse = glm::vec4(color, 1.0f);
    attrs.specular = glm::vec4(0.18f, 0.18f, 0.18f, 1.0f);
    attrs.shininess = shininess;
    return attrs;
}

} // namespace

int main() {
    vkkk::Context ctx;
    GLFWwindow* window = ctx.init_glfw(width, height, "Forward+ Clustered Cornell");
    const auto glfw_extensions = vkkk::Context::get_glfw_instance_extensions();
    ctx.init(window, "vkkk", VK_MAKE_VERSION(1, 0, 0), "vulkan",
        vk::ApiVersion13, true, {}, glfw_extensions);

    vkkk::Scene scene;
    scene.camera = &camera;
    scene.drawable_mgr->add_plane("cornell_plane", {vkkk::VERTEX, vkkk::NORMAL}, 2.0f);
    scene.drawable_mgr->add_cube("cornell_cube", {vkkk::VERTEX, vkkk::NORMAL}, 1.0f);
    scene.drawable_mgr->sync_to_gpu(&ctx);

    vkkk::ForwardPRenderer renderer(&ctx, &scene);
    if (!renderer.initialize(&ctx)) {
        throw std::runtime_error("failed to initialize Forward+ renderer");
    }
    renderer.set_max_lights_per_cluster(16);

    if (!renderer.create_pipeline_from_shader_src(
            pipeline_name,
            vkkk::phong_plus_vert,
            vkkk::phong_plus_frag,
            make_pipeline_option(),
            {vkkk::VERTEX, vkkk::NORMAL}))
    {
        throw std::runtime_error("failed to create clustered phong pipeline");
    }

    vkkk::PhongPlusInstanceAttrs attrs[] = {
        make_instance(
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f))
                * glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            glm::vec3(0.78f), 8.0f),
        make_instance(
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -1.0f))
                * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            glm::vec3(0.78f), 8.0f),
        make_instance(
            glm::translate(glm::mat4(1.0f), glm::vec3(-1.0f, 0.0f, 0.0f))
                * glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            glm::vec3(0.72f, 0.12f, 0.12f), 8.0f),
        make_instance(
            glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f))
                * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            glm::vec3(0.14f, 0.62f, 0.18f), 8.0f),
        make_instance(
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            glm::vec3(0.78f), 8.0f),
        make_instance(
            glm::translate(glm::mat4(1.0f), glm::vec3(-0.45f, -0.6f, -0.15f))
                * glm::rotate(glm::mat4(1.0f), glm::radians(-18.0f), glm::vec3(0.0f, 1.0f, 0.0f))
                * glm::scale(glm::mat4(1.0f), glm::vec3(0.6f, 0.8f, 0.6f)),
            glm::vec3(0.82f), 24.0f),
        make_instance(
            glm::translate(glm::mat4(1.0f), glm::vec3(0.38f, -0.35f, 0.32f))
                * glm::rotate(glm::mat4(1.0f), glm::radians(14.0f), glm::vec3(0.0f, 1.0f, 0.0f))
                * glm::scale(glm::mat4(1.0f), glm::vec3(0.55f, 1.3f, 0.55f)),
            glm::vec3(0.82f), 24.0f),
    };

    constexpr size_t attr_size = sizeof(vkkk::PhongPlusInstanceAttrs);
    for (size_t i = 0; i < 5; ++i) {
        renderer.add_opaque_drawable(
            "cornell_plane", pipeline_name, attr_size, reinterpret_cast<const char*>(&attrs[i]));
    }
    for (size_t i = 5; i < std::size(attrs); ++i) {
        renderer.add_opaque_drawable(
            "cornell_cube", pipeline_name, attr_size, reinterpret_cast<const char*>(&attrs[i]));
    }
    renderer.allocate_ssbo();
    if (!ctx.resize_pipeline_ssbo(pipeline_name, vkkk::buf::PhongInstanceAttrs, std::size(attrs))
        || !ctx.alloc_pipeline_ssbo(pipeline_name, vkkk::buf::PhongInstanceAttrs))
    {
        throw std::runtime_error("failed to allocate instance attributes");
    }

    constexpr uint32_t point_light_count = 64;
    vkkk::PipelineLightStorage lights;
    lights.pt_lights.reserve(point_light_count);
    for (uint32_t i = 0; i < point_light_count; ++i) {
        const uint32_t ix = i % 4;
        const uint32_t iy = (i / 4) % 4;
        const uint32_t iz = i / 16;
        const float t = static_cast<float>(i) / static_cast<float>(point_light_count);

        vkkk::PointLightUBO light{};
        light.vec = glm::vec4(
            -0.75f + static_cast<float>(ix) * 0.5f,
            -0.75f + static_cast<float>(iy) * 0.5f,
            -0.75f + static_cast<float>(iz) * 0.5f,
            1.0f);
        // Dim colored lights so clustering differences stay visible.
        const float hue = t * 6.2831853f;
        const float intensity = 0.08f + 0.12f * (0.5f + 0.5f * std::sin(hue * 1.7f));
        light.color = glm::vec4(
            intensity * (0.55f + 0.45f * std::cos(hue)),
            intensity * (0.55f + 0.45f * std::cos(hue + 2.094f)),
            intensity * (0.55f + 0.45f * std::cos(hue + 4.189f)),
            1.0f);
        // Keep radii small vs 0.5 spacing so typical tile overlap stays under the 16-light cap.
        light.radius = 0.36f + 0.14f * (0.5f + 0.5f * std::sin(hue * 2.3f + 0.4f));
        lights.pt_lights.push_back(light);
    }
    scene.light_mgr->register_pipeline(pipeline_name, lights);

    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_btn_callback);
    glfwSetCursorPosCallback(window, mouse_pos_callback);

    vkkk::ImGuiHud hud;
    if (!hud.init(&ctx)) {
        throw std::runtime_error("failed to initialize imgui hud");
    }

    bool limit_fps_enabled = true;
    float target_fps = 60.0f;
    float current_fps = 0.0f;
    float frame_dt = 0.0f;
    float raw_frame_dt = 0.0f;

    ctx.set_update_cbk([&](uint32_t image_index, float duration) {
        raw_frame_dt = duration;
        camera.ratio = width / static_cast<float>(height);
        camera.update_position(frame_dt);
        camera.update_orientation();
        camera.update_ubo_data();

        hud.begin_frame();
        ImGui::SetNextWindowPos(ImVec2(8.0f, 8.0f), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.35f);
        constexpr ImGuiWindowFlags hud_flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize
            | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing
            | ImGuiWindowFlags_NoNav;
        ImGui::Begin("FPS HUD", nullptr, hud_flags);
        ImGui::Text("Renderer: Forward+ Clustered");
        ImGui::Checkbox("Limit FPS", &limit_fps_enabled);
        ImGui::SliderFloat("Target FPS", &target_fps, 15.0f, 240.0f, "%.0f");
        ImGui::Text("FPS: %.1f", current_fps);
        ImGui::Text("Frame: %.2f ms", frame_dt * 1000.0f);
        ImGui::Text("Raw dt: %.2f ms", raw_frame_dt * 1000.0f);
        ImGui::Text("Plane instances: %d", 5);
        ImGui::Text("Box instances: %d", 2);
        ImGui::Text("Point lights: %u", point_light_count);
        ImGui::End();

        ctx.record_cmds(image_index,
            [&](vk::raii::CommandBuffer& cmd, uint32_t swapchain_index) {
                renderer.record_commands(cmd, swapchain_index);
                hud.render(static_cast<VkCommandBuffer>(*cmd));
            },
            [&](vk::raii::CommandBuffer& cmd, uint32_t swapchain_index) {
                renderer.prepare_light_clusters(cmd, swapchain_index);
            });
    });

    using Clock = std::chrono::steady_clock;
    auto next_frame_tick = Clock::now();
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        const auto frame_begin = Clock::now();
        ctx.draw_frame();
        auto frame_end = Clock::now();

        if (limit_fps_enabled && target_fps > 1.0f) {
            const auto frame_period = std::chrono::duration<float>(1.0f / target_fps);
            next_frame_tick += std::chrono::duration_cast<Clock::duration>(frame_period);
            if (frame_end < next_frame_tick) {
                std::this_thread::sleep_until(next_frame_tick);
                frame_end = Clock::now();
            } else {
                next_frame_tick = frame_end;
            }
        } else {
            next_frame_tick = frame_end;
        }

        frame_dt = std::chrono::duration<float>(frame_end - frame_begin).count();
        current_fps = frame_dt > 0.0f ? 1.0f / frame_dt : 0.0f;
    }

    hud.shutdown();
    ctx.wait_idle();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
