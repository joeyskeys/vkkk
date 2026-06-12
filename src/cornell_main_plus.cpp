#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include "asset_mgr/drawable_mgr.h"
#include "asset_mgr/light_mgr.h"
#include "asset_mgr/scene.h"
#include "concepts/camera.h"
#include "gui/gui.h"
#include "renderer/forward_plus.h"
#include "renderer/renderer.h"
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
    1.333334f,
    0.1f,
    100.0f
};

void key_callback(GLFWwindow* /*win*/, int key, int /*code*/, int action, int /*mods*/) {
    const bool key_down = action == GLFW_PRESS || action == GLFW_REPEAT;
    const bool key_up = action == GLFW_RELEASE;

    if (key == GLFW_KEY_E) {
        cam.y = key_down ? 0.01f : (key_up ? 0.0f : cam.y);
    }
    else if (key == GLFW_KEY_Q) {
        cam.y = key_down ? -0.01f : (key_up ? 0.0f : cam.y);
    }
    else if (key == GLFW_KEY_W) {
        cam.z = key_down ? 0.01f : (key_up ? 0.0f : cam.z);
    }
    else if (key == GLFW_KEY_S) {
        cam.z = key_down ? -0.01f : (key_up ? 0.0f : cam.z);
    }
    else if (key == GLFW_KEY_A) {
        cam.x = key_down ? -0.01f : (key_up ? 0.0f : cam.x);
    }
    else if (key == GLFW_KEY_D) {
        cam.x = key_down ? 0.01f : (key_up ? 0.0f : cam.x);
    }
}

void mouse_btn_callback(GLFWwindow* win, int btn, int action, int /*mods*/) {
    if (btn == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            cam.rotating = true;
            glfwGetCursorPos(win, &cam.prev_x, &cam.prev_y);
        }
        else {
            cam.rotating = false;
        }
    }
}

void mouse_pos_callback(GLFWwindow* /*win*/, double x, double y) {
    if (!cam.rotating) {
        return;
    }

    const float delta_x = static_cast<float>((x - cam.prev_x) / 100.0);
    const float delta_y = static_cast<float>((y - cam.prev_y) / 100.0);
    cam.prev_x = x;
    cam.prev_y = y;
    cam.rotation = glm::angleAxis(delta_x, glm::vec3(0.0f, 1.0f, 0.0f));
    cam.rotation = glm::angleAxis(-delta_y, glm::vec3(1.0f, 0.0f, 0.0f)) * cam.rotation;
}

vkkk::built_in_shader::PhongMaterialUBO make_material(const glm::vec3& color, float shininess = 16.0f) {
    vkkk::built_in_shader::PhongMaterialUBO material{};
    material.ambient = glm::vec4(color * 0.08f, 1.0f);
    material.diffuse = glm::vec4(color, 1.0f);
    material.specular = glm::vec4(0.18f, 0.18f, 0.18f, 1.0f);
    material.shininess = shininess;
    return material;
}

void setup_cornell_scene(vkkk::Scene& scene, vkkk::ForwardPlusRenderer& renderer) {
    scene.drawable_mgr->add_plane("cornell_plane", {vkkk::VERTEX, vkkk::NORMAL}, 2.0f);
    scene.drawable_mgr->add_cube("cornell_cube", {vkkk::VERTEX, vkkk::NORMAL}, 1.0f);
    scene.light_mgr->add_pt_light(
        glm::vec4(0.0f, 0.85f, 0.0f, 1.0f),
        glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    const auto add_plane = [&](const char* pipeline_name, const glm::mat4& model,
                               const glm::vec3& color, float shininess) {
        vkkk::ForwardPlusDrawItem item{};
        item.mesh_name = "cornell_plane";
        item.pipeline_name = pipeline_name;
        item.model = model;
        item.material = make_material(color, shininess);
        renderer.add_draw_item(item);
    };

    const auto add_cube = [&](const char* pipeline_name, const glm::mat4& model,
                              const glm::vec3& color, float shininess) {
        vkkk::ForwardPlusDrawItem item{};
        item.mesh_name = "cornell_cube";
        item.pipeline_name = pipeline_name;
        item.model = model;
        item.material = make_material(color, shininess);
        renderer.add_draw_item(item);
    };

    add_plane("cornell_floor",
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::vec3(0.78f, 0.78f, 0.78f), 8.0f);
    add_plane("cornell_ceiling",
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
            glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        glm::vec3(0.78f, 0.78f, 0.78f), 8.0f);
    add_plane("cornell_back",
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -1.0f)) *
            glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        glm::vec3(0.78f, 0.78f, 0.78f), 8.0f);
    add_plane("cornell_left",
        glm::translate(glm::mat4(1.0f), glm::vec3(-1.0f, 0.0f, 0.0f)) *
            glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
        glm::vec3(0.72f, 0.12f, 0.12f), 8.0f);
    add_plane("cornell_right",
        glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f)) *
            glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
        glm::vec3(0.14f, 0.62f, 0.18f), 8.0f);
    add_cube("cornell_short_box",
        glm::translate(glm::mat4(1.0f), glm::vec3(-0.45f, -0.6f, -0.15f)) *
            glm::rotate(glm::mat4(1.0f), glm::radians(-18.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
            glm::scale(glm::mat4(1.0f), glm::vec3(0.6f, 0.8f, 0.6f)),
        glm::vec3(0.82f, 0.82f, 0.82f), 24.0f);
    add_cube("cornell_tall_box",
        glm::translate(glm::mat4(1.0f), glm::vec3(0.38f, -0.35f, 0.32f)) *
            glm::rotate(glm::mat4(1.0f), glm::radians(14.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
            glm::scale(glm::mat4(1.0f), glm::vec3(0.55f, 1.3f, 0.55f)),
        glm::vec3(0.82f, 0.82f, 0.82f), 24.0f);
}

void upload_mesh(vkkk::Context& ctx, const vkkk::Scene& scene, const std::string& name) {
    const vkkk::Mesh* mesh = scene.drawable_mgr->find_mesh(name);
    if (mesh == nullptr) {
        throw std::runtime_error("mesh not found: " + name);
    }
    if (!ctx.load_mesh(name, *mesh)) {
        throw std::runtime_error("failed to upload mesh: " + name);
    }
}

} // namespace

int main() {
    vkkk::Context ctx;
    GLFWwindow* window = ctx.init_glfw(WIDTH, HEIGHT, "Cornell Box (Forward+)");
    const auto glfw_extensions = vkkk::Context::get_glfw_instance_extensions();
    ctx.init(window, "vkkk", VK_MAKE_VERSION(1, 0, 0), "vulkan", vk::ApiVersion13, true, {}, glfw_extensions);

    vkkk::ForwardPlusRenderer renderer;
    if (!renderer.initialize(&ctx)) {
        throw std::runtime_error("failed to initialize forward+ renderer");
    }
    renderer.set_camera(&cam);

    vkkk::Scene scene;
    setup_cornell_scene(scene, renderer);
    upload_mesh(ctx, scene, "cornell_plane");
    upload_mesh(ctx, scene, "cornell_cube");
    renderer.set_scene(&scene);

    if (!renderer.missing_shaders().empty()) {
        std::cerr << "missing forward+ shaders:";
        for (const auto& shader : renderer.missing_shaders()) {
            std::cerr << ' ' << shader;
        }
        std::cerr << std::endl;
    }

    vkkk::ImGuiHud hud;
    if (!hud.init(&ctx)) {
        throw std::runtime_error("failed to initialize imgui hud");
    }
    renderer.set_overlay_draw([&](vk::CommandBuffer cmd) {
        hud.render(static_cast<VkCommandBuffer>(cmd));
    });

    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_btn_callback);
    glfwSetCursorPosCallback(window, mouse_pos_callback);

    bool limit_fps_enabled = true;
    float target_fps = 60.0f;
    float current_fps = 0.0f;
    float frame_dt = 0.0f;
    float raw_frame_dt = 0.0f;

    ctx.set_update_cbk([&](uint32_t idx, float duration) {
        raw_frame_dt = duration;
        cam.update_position(frame_dt);
        cam.update_orientation();

        vkkk::RenderView view{};
        view.swapchain_image_idx = idx;
        view.delta_seconds = frame_dt;
        renderer.update(view);

        hud.begin_frame();
        ImGui::SetNextWindowPos(ImVec2(8.0f, 8.0f), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.35f);
        constexpr ImGuiWindowFlags hud_flags =
            ImGuiWindowFlags_NoDecoration
            | ImGuiWindowFlags_AlwaysAutoResize
            | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoFocusOnAppearing
            | ImGuiWindowFlags_NoNav;
        ImGui::Begin("FPS HUD", nullptr, hud_flags);
        ImGui::Text("Renderer: %s", renderer.type_name());
        ImGui::Checkbox("Limit FPS", &limit_fps_enabled);
        ImGui::SliderFloat("Target FPS", &target_fps, 15.0f, 240.0f, "%.0f");
        ImGui::Text("FPS: %.1f", current_fps);
        ImGui::Text("Frame: %.2f ms", frame_dt * 1000.0f);
        ImGui::Text("Raw dt: %.2f ms", raw_frame_dt * 1000.0f);
        ImGui::End();

        ctx.record_cmds(idx, [&](vk::raii::CommandBuffer& cmd_buf, uint32_t image_index) {
            (void)image_index;
            renderer.record_commands(static_cast<vk::CommandBuffer>(*cmd_buf), view);
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
            }
            else {
                next_frame_tick = frame_end;
            }
        }
        else {
            next_frame_tick = frame_end;
        }

        frame_dt = std::chrono::duration<float>(frame_end - frame_begin).count();
        current_fps = frame_dt > 0.0f ? 1.0f / frame_dt : 0.0f;
    }

    renderer.shutdown();
    hud.shutdown();
    ctx.wait_idle();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
