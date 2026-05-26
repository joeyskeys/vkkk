#include <array>
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

#include "asset_mgr/scene.h"
#include "asset_mgr/drawable_mgr.h"
#include "asset_mgr/light_mgr.h"
#include "built_in_shader/phong.h"
#include "concepts/camera.h"
#include "gui/gui.h"
#include "renderer/deferred.h"
#include "renderer/forward.h"
#include "renderer/renderer.h"
#include "vk_ins/cmd_buf.h"
#include "vk_ins/vkabstraction.h"

namespace
{

using vkkk::ForwardDrawItem;
using vkkk::ForwardRenderer;
using vkkk::RenderView;
using vkkk::Renderer;
using vkkk::built_in_shader::PhongMaterialUBO;

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

PhongMaterialUBO make_material(const glm::vec3& color, float shininess = 16.0f) {
    PhongMaterialUBO material{};
    material.ambient = glm::vec4(color * 0.08f, 1.0f);
    material.diffuse = glm::vec4(color, 1.0f);
    material.specular = glm::vec4(0.18f, 0.18f, 0.18f, 1.0f);
    material.shininess = shininess;
    return material;
}

std::unique_ptr<Renderer> create_renderer(const std::string& type) {
    if (type == "forward") {
        return std::make_unique<ForwardRenderer>();
    }
    if (type == "deferred") {
        return std::make_unique<vkkk::DeferredRenderer>();
    }
    throw std::runtime_error("unknown renderer type: " + type);
}

void setup_cornell_scene(vkkk::Scene& scene, ForwardRenderer& renderer) {
    scene.drawable_mgr->add_plane("cornell_plane", {vkkk::VERTEX, vkkk::NORMAL}, 2.0f);
    scene.drawable_mgr->add_cube("cornell_cube", {vkkk::VERTEX, vkkk::NORMAL}, 1.0f);
    scene.light_mgr->add_pt_light(
        glm::vec4(0.0f, 0.85f, 0.0f, 1.0f),
        glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    const auto add_plane = [&](const glm::mat4& model, const glm::vec3& color, float shininess) {
        ForwardDrawItem item{};
        item.mesh_name = "cornell_plane";
        item.model = model;
        item.material = make_material(color, shininess);
        renderer.add_draw_item(item);
    };

    const auto add_cube = [&](const glm::mat4& model, const glm::vec3& color, float shininess) {
        ForwardDrawItem item{};
        item.mesh_name = "cornell_cube";
        item.model = model;
        item.material = make_material(color, shininess);
        renderer.add_draw_item(item);
    };

    add_plane(
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::vec3(0.78f, 0.78f, 0.78f),
        8.0f);
    add_plane(
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
            glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        glm::vec3(0.78f, 0.78f, 0.78f),
        8.0f);
    add_plane(
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -1.0f)) *
            glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        glm::vec3(0.78f, 0.78f, 0.78f),
        8.0f);
    add_plane(
        glm::translate(glm::mat4(1.0f), glm::vec3(-1.0f, 0.0f, 0.0f)) *
            glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
        glm::vec3(0.72f, 0.12f, 0.12f),
        8.0f);
    add_plane(
        glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f)) *
            glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
        glm::vec3(0.14f, 0.62f, 0.18f),
        8.0f);
    add_cube(
        glm::translate(glm::mat4(1.0f), glm::vec3(-0.45f, -0.6f, -0.15f)) *
            glm::rotate(glm::mat4(1.0f), glm::radians(-18.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
            glm::scale(glm::mat4(1.0f), glm::vec3(0.6f, 0.8f, 0.6f)),
        glm::vec3(0.82f, 0.82f, 0.82f),
        24.0f);
    add_cube(
        glm::translate(glm::mat4(1.0f), glm::vec3(0.38f, -0.35f, 0.32f)) *
            glm::rotate(glm::mat4(1.0f), glm::radians(14.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
            glm::scale(glm::mat4(1.0f), glm::vec3(0.55f, 1.3f, 0.55f)),
        glm::vec3(0.82f, 0.82f, 0.82f),
        24.0f);
}

std::string parse_renderer_type(int argc, char** argv) {
    std::string type = "forward";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--renderer" && i + 1 < argc) {
            type = argv[++i];
        }
    }
    return type;
}

} // namespace

int main(int argc, char** argv) {
    const std::string renderer_type = parse_renderer_type(argc, argv);

    vkkk::VkWrappedInstance ins;
    ins.init_glfw();
    ins.init();
    ins.list_physical_devices();
    ins.create_resources(VK_SAMPLE_COUNT_8_BIT);

    auto renderer = create_renderer(renderer_type);
    if (!renderer->initialize(&ins)) {
        throw std::runtime_error("failed to initialize renderer: " + renderer_type);
    }

    vkkk::Scene scene;
    ForwardRenderer* forward_renderer = nullptr;
    if (auto* forward = dynamic_cast<ForwardRenderer*>(renderer.get())) {
        forward_renderer = forward;
        forward->set_camera(&cam);
        setup_cornell_scene(scene, *forward);

        if (!forward->missing_shaders().empty()) {
            std::cerr << "missing forward shaders:";
            for (const auto& shader : forward->missing_shaders()) {
                std::cerr << ' ' << shader;
            }
            std::cerr << std::endl;
        }
    }

    scene.drawable_mgr->upload_gpu(&ins, "cornell_plane");
    scene.drawable_mgr->upload_gpu(&ins, "cornell_cube");
    renderer->set_scene(&scene);

    vkkk::ImGuiHud hud;
    if (!hud.init(&ins)) {
        throw std::runtime_error("failed to initialize imgui hud");
    }

    if (forward_renderer) {
        forward_renderer->set_overlay_draw([&](VkCommandBuffer cmd) {
            hud.render(cmd);
        });
    }

    ins.setup_key_cbk(key_callback);
    ins.setup_mouse_btn_cbk(mouse_btn_callback);
    ins.setup_mouse_pos_cbk(mouse_pos_callback);

    vkkk::CommandBuffers cmd_bufs(&ins);
    cmd_bufs.alloc();

    bool limit_fps_enabled = true;
    float target_fps = 60.0f;
    float current_fps = 0.0f;
    float frame_dt = 0.0f;
    float raw_frame_dt = 0.0f;

    ins.set_update_cbk([&](uint32_t idx, float duration) {
        raw_frame_dt = duration;

        cam.update_position(frame_dt);
        cam.update_orientation();

        RenderView view{};
        view.swapchain_image_idx = idx;
        view.delta_seconds = frame_dt;
        renderer->update(view);

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
        ImGui::Text("Renderer: %s", renderer->type_name());
        ImGui::Checkbox("Limit FPS", &limit_fps_enabled);
        ImGui::SliderFloat("Target FPS", &target_fps, 15.0f, 240.0f, "%.0f");
        ImGui::Text("FPS: %.1f", current_fps);
        ImGui::Text("Frame: %.2f ms", frame_dt * 1000.0f);
        ImGui::Text("Raw dt: %.2f ms", raw_frame_dt * 1000.0f);
        ImGui::End();

        auto& cmd = cmd_bufs[idx];
        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (vkBeginCommandBuffer(cmd, &begin_info) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer");
        }

        renderer->record_commands(cmd, view);

        if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer");
        }
    });

    ins.create_sync_objects();

    using Clock = std::chrono::steady_clock;
    auto next_frame_tick = Clock::now();
    while (!glfwWindowShouldClose(ins.get_window())) {
        glfwPollEvents();

        // VkWrappedInstance uses one MSAA color/depth pair for all swapchain images.
        // With MAX_FRAMES_IN_FLIGHT > 1, overlapping submits write those attachments
        // without a barrier (SYNC-HAZARD-WRITE-AFTER-WRITE). Serialize GPU work here
        // until the instance owns per-flight MSAA targets.
        vkDeviceWaitIdle(ins.get_device());

        const auto frame_begin = Clock::now();
        ins.draw_frame(cmd_bufs);
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

    vkDeviceWaitIdle(ins.get_device());
    renderer->shutdown();
    hud.shutdown();

    return 0;
}
