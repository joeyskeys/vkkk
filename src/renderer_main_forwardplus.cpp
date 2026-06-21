#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include "asset_mgr/drawable_mgr.h"
#include "asset_mgr/light_mgr.h"
#include "asset_mgr/scene.h"
#include "built_in_shader/phong.h"
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

int g_draw_mode = 1; // 0=wireframe, 1=shaded, 2=shaded_wireframe

struct DrawModeUBO {
    int mode = 1;
    int _pad0 = 0;
    int _pad1 = 0;
    int _pad2 = 0;
};

void sync_draw_mode_uniforms(vkkk::Context& ctx, uint32_t swapchain_idx) {
    DrawModeUBO mode_ubo{};
    mode_ubo.mode = g_draw_mode;
    for (auto& [name, ubo] : ctx.ubos) {
        const bool matches_block_name =
            name.size() >= 9 && name.substr(name.size() - 9) == ":DrawMode";
        const bool matches_instance_name =
            name.size() >= 10 && name.substr(name.size() - 10) == ":draw_mode";
        if (!matches_block_name && !matches_instance_name) {
            continue;
        }
        if (swapchain_idx >= ubo.memos.size()) {
            continue;
        }
        ctx.sync_uniform(ubo.memos[swapchain_idx], &mode_ubo, static_cast<uint32_t>(sizeof(mode_ubo)));
    }
}

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
    else if (action == GLFW_PRESS && key == GLFW_KEY_1) {
        g_draw_mode = 0; // wireframe
    }
    else if (action == GLFW_PRESS && key == GLFW_KEY_2) {
        g_draw_mode = 1; // shaded
    }
    else if (action == GLFW_PRESS && key == GLFW_KEY_3) {
        g_draw_mode = 2; // shaded + wireframe
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

struct MaterialUBO : public vkkk::UBOBase {
    vkkk::PhongMaterialUBO data{};

    explicit MaterialUBO(const vkkk::PhongMaterialUBO& material) : data(material) {}

    size_t size() const override {
        return sizeof(data);
    }
};

vkkk::PhongMaterialUBO make_material(const glm::vec3& color, float shininess = 16.0f) {
    vkkk::PhongMaterialUBO material{};
    material.ambient = glm::vec4(color * 0.08f, 1.0f);
    material.diffuse = glm::vec4(color, 1.0f);
    material.specular = glm::vec4(0.18f, 0.18f, 0.18f, 1.0f);
    material.shininess = shininess;
    return material;
}

void setup_cornell_scene(vkkk::Scene& scene, std::vector<std::unique_ptr<MaterialUBO>>& materials) {
    scene.drawable_mgr->add_plane("cornell_plane", {vkkk::VERTEX, vkkk::NORMAL}, 2.0f);
    scene.drawable_mgr->add_cube("cornell_cube", {vkkk::VERTEX, vkkk::NORMAL}, 1.0f);
    scene.light_mgr->add_pt_light(
        glm::vec4(0.0f, 0.85f, 0.0f, 1.0f),
        glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    materials.push_back(std::make_unique<MaterialUBO>(make_material(glm::vec3(0.78f, 0.78f, 0.78f), 8.0f)));
    scene.drawable_mgr->setup_drawable(
        "cornell_floor", "cornell_plane", "cornell_floor",
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        false, materials.back().get());

    materials.push_back(std::make_unique<MaterialUBO>(make_material(glm::vec3(0.78f, 0.78f, 0.78f), 8.0f)));
    scene.drawable_mgr->setup_drawable(
        "cornell_ceiling", "cornell_plane", "cornell_ceiling",
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
            glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        false, materials.back().get());

    materials.push_back(std::make_unique<MaterialUBO>(make_material(glm::vec3(0.78f, 0.78f, 0.78f), 8.0f)));
    scene.drawable_mgr->setup_drawable(
        "cornell_back", "cornell_plane", "cornell_back",
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -1.0f)) *
            glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        false, materials.back().get());

    materials.push_back(std::make_unique<MaterialUBO>(make_material(glm::vec3(0.72f, 0.12f, 0.12f), 8.0f)));
    scene.drawable_mgr->setup_drawable(
        "cornell_left", "cornell_plane", "cornell_left",
        glm::translate(glm::mat4(1.0f), glm::vec3(-1.0f, 0.0f, 0.0f)) *
            glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
        false, materials.back().get());

    materials.push_back(std::make_unique<MaterialUBO>(make_material(glm::vec3(0.14f, 0.62f, 0.18f), 8.0f)));
    scene.drawable_mgr->setup_drawable(
        "cornell_right", "cornell_plane", "cornell_right",
        glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f)) *
            glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
        false, materials.back().get());

    materials.push_back(std::make_unique<MaterialUBO>(make_material(glm::vec3(0.82f, 0.82f, 0.82f), 24.0f)));
    scene.drawable_mgr->setup_drawable(
        "cornell_short_box", "cornell_cube", "cornell_short_box",
        glm::translate(glm::mat4(1.0f), glm::vec3(-0.45f, -0.6f, -0.15f)) *
            glm::rotate(glm::mat4(1.0f), glm::radians(-18.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
            glm::scale(glm::mat4(1.0f), glm::vec3(0.6f, 0.8f, 0.6f)),
        false, materials.back().get());

    materials.push_back(std::make_unique<MaterialUBO>(make_material(glm::vec3(0.82f, 0.82f, 0.82f), 24.0f)));
    scene.drawable_mgr->setup_drawable(
        "cornell_tall_box", "cornell_cube", "cornell_tall_box",
        glm::translate(glm::mat4(1.0f), glm::vec3(0.38f, -0.35f, 0.32f)) *
            glm::rotate(glm::mat4(1.0f), glm::radians(14.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
            glm::scale(glm::mat4(1.0f), glm::vec3(0.55f, 1.3f, 0.55f)),
        false, materials.back().get());
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
    ctx.use_mesh_shader = true;
    GLFWwindow* window = ctx.init_glfw(WIDTH, HEIGHT, "Forward+ Renderer Main (HPP)");
    const auto glfw_extensions = vkkk::Context::get_glfw_instance_extensions();
    ctx.init(window, "vkkk", VK_MAKE_VERSION(1, 0, 0), "vulkan", vk::ApiVersion13, true, {}, glfw_extensions);

    vkkk::ForwardPlusRenderer renderer;
    if (!renderer.initialize(&ctx)) {
        throw std::runtime_error("failed to initialize forward+ renderer");
    }
    renderer.set_camera(&cam);

    vkkk::Scene scene;
    std::vector<std::unique_ptr<MaterialUBO>> cornell_materials;
    setup_cornell_scene(scene, cornell_materials);
    upload_mesh(ctx, scene, "cornell_plane");
    upload_mesh(ctx, scene, "cornell_cube");
    renderer.set_scene(&scene);

    vkkk::ImGuiHud hud;
    if (!hud.init(&ctx)) {
        throw std::runtime_error("failed to initialize imgui hud");
    }

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
        sync_draw_mode_uniforms(ctx, idx);

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
        const char* draw_mode_label = "Shaded";
        if (g_draw_mode == 0) {
            draw_mode_label = "Wireframe";
        }
        else if (g_draw_mode == 2) {
            draw_mode_label = "Shaded + Wireframe";
        }
        ImGui::Text("Draw Mode: %s", draw_mode_label);
        ImGui::Text("Hotkeys: 1/2/3");
        ImGui::End();

        ctx.record_cmds(idx, [&](vk::raii::CommandBuffer& cmd_buf, uint32_t image_index) {
            (void)image_index;
            renderer.record_commands(static_cast<vk::CommandBuffer>(*cmd_buf), view);
            hud.render(static_cast<VkCommandBuffer>(*cmd_buf));
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
