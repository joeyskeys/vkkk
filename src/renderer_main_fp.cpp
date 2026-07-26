#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include "renderer/forwardp.hpp"
#include "asset_mgr/drawable_mgr.h"
#include "asset_mgr/light_mgr.h"
#include "asset_mgr/scene.h"
#include "built_in_shader/phong.h"
#include "concepts/camera.h"
#include "gui/gui.h"
#include "vk_ins/context.hpp"

namespace
{

constexpr unsigned int WIDTH = 800;
constexpr unsigned int HEIGHT = 600;

vkkk::Camera cam{
    glm::vec3{0.0f, 0.0f, 3.8f},
    glm::vec3{0.0f, 0.0f, -1.0f},
    glm::vec3{0.0f, 1.0f, 0.0f},
    35.0f,
    WIDTH / static_cast<float>(HEIGHT),
    0.1f,
    100.0f
};

void key_callback(GLFWwindow* /*win*/, int key, int /*code*/, int action, int /*mods*/) {
    const bool key_down = action == GLFW_PRESS || action == GLFW_REPEAT;
    const bool key_up = action == GLFW_RELEASE;
    if (key == GLFW_KEY_E) cam.y = key_down ? 0.01f : (key_up ? 0.0f : cam.y);
    else if (key == GLFW_KEY_Q) cam.y = key_down ? -0.01f : (key_up ? 0.0f : cam.y);
    else if (key == GLFW_KEY_W) cam.z = key_down ? 0.01f : (key_up ? 0.0f : cam.z);
    else if (key == GLFW_KEY_S) cam.z = key_down ? -0.01f : (key_up ? 0.0f : cam.z);
    else if (key == GLFW_KEY_A) cam.x = key_down ? -0.01f : (key_up ? 0.0f : cam.x);
    else if (key == GLFW_KEY_D) cam.x = key_down ? 0.01f : (key_up ? 0.0f : cam.x);
}

void mouse_btn_callback(GLFWwindow* win, int btn, int action, int /*mods*/) {
    if (btn == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            cam.rotating = true;
            glfwGetCursorPos(win, &cam.prev_x, &cam.prev_y);
        } else {
            cam.rotating = false;
        }
    }
}

void mouse_pos_callback(GLFWwindow* /*win*/, double x, double y) {
    if (!cam.rotating) return;
    const float delta_x = static_cast<float>((x - cam.prev_x) / 100.0);
    const float delta_y = static_cast<float>((y - cam.prev_y) / 100.0);
    cam.prev_x = x;
    cam.prev_y = y;
    cam.rotation = glm::angleAxis(delta_x, glm::vec3(0.0f, 1.0f, 0.0f));
    cam.rotation = glm::angleAxis(-delta_y, glm::vec3(1.0f, 0.0f, 0.0f)) * cam.rotation;
}

vkkk::PipelineOption make_pipeline_option() {
    vkkk::PipelineOption option;
    option.setup_multisampling(false, vk::SampleCountFlagBits::e1);
    option.setup_rasterizer(false, false, vk::PolygonMode::eFill, 1.0f,
        vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise, false);
    option.setup_depth_stencil(true, true, vk::CompareOp::eLess, false, false);
    option.setup_viewport(0.0f, 0.0f, static_cast<float>(WIDTH), static_cast<float>(HEIGHT), 0.0f, 1.0f);
    option.setup_scissor(0, 0, WIDTH, HEIGHT);
    return option;
}

glm::vec4 phong_ambient(const glm::vec3& color) {
    return glm::vec4(color * 0.08f, 1.0f);
}

} // namespace

int main() {
    vkkk::Context ctx;
    GLFWwindow* window = ctx.init_glfw(WIDTH, HEIGHT, "Forward+ Cornell (Instanced)");
    const auto glfw_extensions = vkkk::Context::get_glfw_instance_extensions();
    ctx.init(window, "vkkk", VK_MAKE_VERSION(1, 0, 0), "vulkan", vk::ApiVersion13, true, {}, glfw_extensions);

    vkkk::Scene scene;
    scene.camera = &cam;
    scene.drawable_mgr->add_plane("cornell_plane", {vkkk::VERTEX, vkkk::NORMAL}, 2.0f);
    scene.drawable_mgr->add_cube("cornell_cube", {vkkk::VERTEX, vkkk::NORMAL}, 1.0f);
    scene.drawable_mgr->sync_to_gpu(&ctx);

    vkkk::ForwardPRenderer renderer(&ctx, &scene);
    renderer.initialize(&ctx);

    if (!renderer.create_pipeline_from_shader_src(
            "phong_mat",
            vkkk::phong_vert,
            vkkk::phong_frag,
            make_pipeline_option(),
            {vkkk::VERTEX, vkkk::NORMAL})) {
        throw std::runtime_error("failed to create phong_mat pipeline");
    }

    constexpr auto phong_attr_size = sizeof(vkkk::built_in_shader::PhongInstanceAttrs);
    vkkk::built_in_shader::PhongInstanceAttrs phong_attrs[7] = {
        {
            .model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f)) * glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            .ambient = phong_ambient(glm::vec3(0.78f)),
            .diffuse = glm::vec4(0.78f, 0.78f, 0.78f, 1.0f),
            .specular = glm::vec4(0.18f, 0.18f, 0.18f, 1.0f),
            .shininess = 8.0f
        },
        {
            .model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -1.0f)) * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            .ambient = phong_ambient(glm::vec3(0.78f)),
            .diffuse = glm::vec4(0.78f, 0.78f, 0.78f, 1.0f),
            .specular = glm::vec4(0.18f, 0.18f, 0.18f, 1.0f),
            .shininess = 8.0f
        },
        {
            .model = glm::translate(glm::mat4(1.0f), glm::vec3(-1.0f, 0.0f, 0.0f)) * glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            .ambient = phong_ambient(glm::vec3(0.72f, 0.12f, 0.12f)),
            .diffuse = glm::vec4(0.72f, 0.12f, 0.12f, 1.0f),
            .specular = glm::vec4(0.12f, 0.04f, 0.04f, 1.0f),
            .shininess = 8.0f
        },
        {
            .model = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            .ambient = phong_ambient(glm::vec3(0.14f, 0.62f, 0.18f)),
            .diffuse = glm::vec4(0.14f, 0.62f, 0.18f, 1.0f),
            .specular = glm::vec4(0.06f, 0.25f, 0.07f, 1.0f),
            .shininess = 8.0f
        },
        {
            .model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            .ambient = phong_ambient(glm::vec3(0.78f)),
            .diffuse = glm::vec4(0.78f, 0.78f, 0.78f, 1.0f),
            .specular = glm::vec4(0.18f, 0.18f, 0.18f, 1.0f),
            .shininess = 8.0f
        },
        {
            .model = glm::translate(glm::mat4(1.0f), glm::vec3(-0.45f, -0.6f, -0.15f)) *
                glm::rotate(glm::mat4(1.0f), glm::radians(-18.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
                glm::scale(glm::mat4(1.0f), glm::vec3(0.6f, 0.8f, 0.6f)),
            .ambient = phong_ambient(glm::vec3(0.82f)),
            .diffuse = glm::vec4(0.82f, 0.82f, 0.82f, 1.0f),
            .specular = glm::vec4(0.18f, 0.18f, 0.18f, 1.0f),
            .shininess = 24.0f
        },
        {
            .model = glm::translate(glm::mat4(1.0f), glm::vec3(0.38f, -0.35f, 0.32f)) *
                glm::rotate(glm::mat4(1.0f), glm::radians(14.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
                glm::scale(glm::mat4(1.0f), glm::vec3(0.55f, 1.3f, 0.55f)),
            .ambient = phong_ambient(glm::vec3(0.82f)),
            .diffuse = glm::vec4(0.82f, 0.82f, 0.82f, 1.0f),
            .specular = glm::vec4(0.18f, 0.18f, 0.18f, 1.0f),
            .shininess = 24.0f
        }
    };
    renderer.add_opaque_drawable(
        "cornell_plane",
        "phong_mat",
		phong_attr_size,
        reinterpret_cast<char*>(&phong_attrs[0]));
    renderer.add_opaque_drawable(
        "cornell_plane",
        "phong_mat",
        phong_attr_size,
        reinterpret_cast<char*>(&phong_attrs[1]));
    renderer.add_opaque_drawable(
        "cornell_plane",
        "phong_mat",
        phong_attr_size,
        reinterpret_cast<char*>(&phong_attrs[2]));
    renderer.add_opaque_drawable(
        "cornell_plane",
        "phong_mat",
        phong_attr_size,
        reinterpret_cast<char*>(&phong_attrs[3]));
    renderer.add_opaque_drawable(
        "cornell_plane",
        "phong_mat",
        phong_attr_size,
        reinterpret_cast<char*>(&phong_attrs[4]));
    renderer.add_opaque_drawable(
        "cornell_cube",
        "phong_mat",
        phong_attr_size,
        reinterpret_cast<char*>(&phong_attrs[5]));
    renderer.add_opaque_drawable(
        "cornell_cube",
        "phong_mat",
        phong_attr_size,
        reinterpret_cast<char*>(&phong_attrs[6]));
    renderer.allocate_ssbo();
    if (!ctx.resize_pipeline_ssbo("phong_mat", 7)) {
        throw std::runtime_error("failed to set phong_mat ssbo capacity");
    }
    if (!ctx.alloc_pipeline_ssbo("phong_mat")) {
        throw std::runtime_error("failed to allocate phong_mat ssbo gpu buffers");
    }
    vkkk::PipelineLightStorage light_storage;
    vkkk::PointLightUBO point_light{};
    point_light.vec = glm::vec4(0.0f, 0.85f, 0.0f, 1.0f);
    point_light.color = glm::vec4(1.0f);
    light_storage.pt_lights.push_back(point_light);
    scene.light_mgr->register_pipeline("phong_mat", light_storage);

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

    ctx.set_update_cbk([&](uint32_t idx, float duration) {
        raw_frame_dt = duration;
        cam.ratio = WIDTH / static_cast<float>(HEIGHT);
        cam.update_position(frame_dt);
        cam.update_orientation();
        cam.update_ubo_data();

        hud.begin_frame();
        ImGui::SetNextWindowPos(ImVec2(8.0f, 8.0f), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.35f);
        constexpr ImGuiWindowFlags hud_flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize
            | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing
            | ImGuiWindowFlags_NoNav;
        ImGui::Begin("FPS HUD", nullptr, hud_flags);
        ImGui::Text("Renderer: Forward+ (Instanced)");
        ImGui::Checkbox("Limit FPS", &limit_fps_enabled);
        ImGui::SliderFloat("Target FPS", &target_fps, 15.0f, 240.0f, "%.0f");
        ImGui::Text("FPS: %.1f", current_fps);
        ImGui::Text("Frame: %.2f ms", frame_dt * 1000.0f);
        ImGui::Text("Raw dt: %.2f ms", raw_frame_dt * 1000.0f);
        ImGui::Text("Plane instances: %d", 5);
        ImGui::Text("Box instances: %d", 2);
        ImGui::End();

        ctx.begin_cmds(idx);
        auto& cmd_buf = ctx.command_buffers[idx];
        renderer.prepare_light_clusters(cmd_buf, idx);
        const vkkk::PassDesc pass{};
        ctx.begin_pass(cmd_buf, idx, pass);
        renderer.record_commands(cmd_buf, idx);
        hud.render(static_cast<VkCommandBuffer>(*cmd_buf));
        ctx.end_pass(cmd_buf, idx, pass);
        ctx.end_cmds(idx);
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
