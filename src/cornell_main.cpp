#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include "asset_mgr/scene.h"
#include "asset_mgr/light_mgr.h"
#include "asset_mgr/drawable_mgr.h"
#include "built_in_shader/built_in_shader_mgr.h"
#include "built_in_shader/fixed_color.h"
#include "built_in_shader/phong.h"
#include "concepts/camera.h"
#include "gui/gui.h"
#include "vk_ins/cmd_buf.h"
#include "vk_ins/vkabstraction.h"

const static unsigned int WIDTH = 800;
const static unsigned int HEIGHT = 600;

const std::array<const char*, 1> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

vkkk::Camera cam{
    glm::vec3{0.0f, 0.0f, 3.8f},
    glm::vec3{0.0f, 0.0f, -1.0f},
    glm::vec3{0.0f, 1.0f, 0.0f},
    35.0f,
    1.333334f,
    0.1f,
    100.0f
};

void key_callback(GLFWwindow* win, int key, int code, int action, int mods) {
    const bool key_down = action == GLFW_PRESS || action == GLFW_REPEAT;
    const bool key_up = action == GLFW_RELEASE;

    if (key == GLFW_KEY_E) {
        if (key_down)
            cam.y = .01f;
        else if (key_up)
            cam.y = 0.f;
    }
    else if (key == GLFW_KEY_Q) {
        if (key_down)
            cam.y = -.01f;
        else if (key_up)
            cam.y = 0.f;
    }
    else if (key == GLFW_KEY_W) {
        if (key_down)
            cam.z = .01f;
        else if (key_up)
            cam.z = 0.f;
    }
    else if (key == GLFW_KEY_S) {
        if (key_down)
            cam.z = -.01f;
        else if (key_up)
            cam.z = 0.f;
    }
    else if (key == GLFW_KEY_A) {
        if (key_down)
            cam.x = -.01f;
        else if (key_up)
            cam.x = 0.f;
    }
    else if (key == GLFW_KEY_D) {
        if (key_down)
            cam.x = .01f;
        else if (key_up)
            cam.x = 0.f;
    }
}

void mouse_btn_callback(GLFWwindow* win, int btn, int action, int mods) {
    if (btn == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            cam.rotating = true;
            glfwGetCursorPos(win, &cam.prev_x, &cam.prev_y);
        }
        else
            cam.rotating = false;
    }
}

void mouse_pos_callback(GLFWwindow* win, double x, double y) {
    if (cam.rotating) {
        float delta_x = (x - cam.prev_x) / 100.0;
        float delta_y = (y - cam.prev_y) / 100.0;
        cam.prev_x = x;
        cam.prev_y = y;
        cam.rotation = glm::angleAxis(delta_x, glm::vec3(0, 1, 0));
        cam.rotation = glm::angleAxis(-delta_y, glm::vec3(1, 0, 0)) * cam.rotation;
    }
}

namespace
{

using vkkk::built_in_shader::PhongLightUBO;
using vkkk::built_in_shader::PhongMaterialUBO;
using vkkk::built_in_shader::PhongTransformUBO;
using vkkk::built_in_shader::FixedColorUBO;
using vkkk::built_in_shader::FixedColorTransformUBO;

struct CornellRenderable {
    vkkk::built_in_shader::BuiltInShaderType shader_type{
        vkkk::built_in_shader::BuiltInShaderType::Phong
    };
    std::string mesh_name;
    std::string pipeline_name;
    glm::mat4 model{1.0f};
    PhongMaterialUBO material{};
    FixedColorUBO fixed_color{};
};

void update_renderable_uniforms(vkkk::VkWrappedInstance& ins, const CornellRenderable& renderable,
    uint32_t swapchain_idx, const PhongLightUBO& light_ubo)
{
    if (renderable.shader_type == vkkk::built_in_shader::BuiltInShaderType::Phong) {
        PhongTransformUBO transform_ubo{};
        transform_ubo.model = renderable.model;
        transform_ubo.view = cam.get_view_mat();
        transform_ubo.proj = cam.get_proj_mat();

        auto& transform = ins.require_ubo(renderable.pipeline_name + ":ubo");
        auto& material = ins.require_ubo(renderable.pipeline_name + ":material");
        auto& light = ins.require_ubo(renderable.pipeline_name + ":light");

        ins.sync_uniform(transform.memos[swapchain_idx], &transform_ubo, sizeof(transform_ubo));
        ins.sync_uniform(material.memos[swapchain_idx], &renderable.material, sizeof(renderable.material));
        ins.sync_uniform(light.memos[swapchain_idx], &light_ubo, sizeof(light_ubo));
    }
    else if (renderable.shader_type == vkkk::built_in_shader::BuiltInShaderType::FixedColor) {
        FixedColorTransformUBO transform_ubo{};
        transform_ubo.model = renderable.model;
        transform_ubo.view = cam.get_view_mat();
        transform_ubo.proj = cam.get_proj_mat();

        auto& transform = ins.require_ubo(renderable.pipeline_name + ":ubo");
        auto& fixed_color = ins.require_ubo(renderable.pipeline_name + ":fixed_color");

        ins.sync_uniform(transform.memos[swapchain_idx], &transform_ubo, sizeof(transform_ubo));
        ins.sync_uniform(fixed_color.memos[swapchain_idx], &renderable.fixed_color, sizeof(renderable.fixed_color));
    }
}

PhongMaterialUBO make_material(const glm::vec3& color, float shininess = 16.0f) {
    PhongMaterialUBO material{};
    material.ambient = glm::vec4(color * 0.08f, 1.0f);
    material.diffuse = glm::vec4(color, 1.0f);
    material.specular = glm::vec4(0.18f, 0.18f, 0.18f, 1.0f);
    material.shininess = shininess;
    return material;
}

} // namespace

int main() {
    vkkk::VkWrappedInstance ins;
    ins.init_glfw();
    ins.init();
    ins.list_physical_devices();
    ins.create_resources(VK_SAMPLE_COUNT_8_BIT);

    vkkk::Scene scene;
    scene.drawable_mgr->add_plane("cornell_plane", {vkkk::VERTEX, vkkk::NORMAL}, 2.0f);
    scene.drawable_mgr->add_cube("cornell_cube", {vkkk::VERTEX, vkkk::NORMAL}, 1.0f);
    scene.drawable_mgr->upload_gpu(&ins, "cornell_plane");
    scene.drawable_mgr->upload_gpu(&ins, "cornell_cube");

    // Keep scene-level light resources alongside drawables for future extensibility.
    scene.light_mgr->add_pt_light(glm::vec4(0.0f, 0.85f, 0.0f, 1.0f), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    vkkk::built_in_shader::BuiltInShaderMgr shader_mgr(&ins);
    vkkk::PipelineOption ppl_opt;
    ppl_opt.setup_multisampling(true, ins.nsample);
    ppl_opt.setup_rasterizer(false, false, VK_POLYGON_MODE_FILL, 1.0f, VK_CULL_MODE_NONE,
        VK_FRONT_FACE_COUNTER_CLOCKWISE, false);

    std::vector<CornellRenderable> renderables{
        CornellRenderable{
            .mesh_name = "cornell_plane",
            .pipeline_name = "cornell_floor",
            .model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            .material = make_material(glm::vec3(0.78f, 0.78f, 0.78f), 8.0f)
        },
        CornellRenderable{
            .mesh_name = "cornell_plane",
            .pipeline_name = "cornell_ceiling",
            .model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
                glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            .material = make_material(glm::vec3(0.78f, 0.78f, 0.78f), 8.0f)
        },
        CornellRenderable{
            .mesh_name = "cornell_plane",
            .pipeline_name = "cornell_back",
            .model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -1.0f)) *
                glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            .material = make_material(glm::vec3(0.78f, 0.78f, 0.78f), 8.0f)
        },
        CornellRenderable{
            .mesh_name = "cornell_plane",
            .pipeline_name = "cornell_left",
            .model = glm::translate(glm::mat4(1.0f), glm::vec3(-1.0f, 0.0f, 0.0f)) *
                glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            .material = make_material(glm::vec3(0.72f, 0.12f, 0.12f), 8.0f),
            .fixed_color = FixedColorUBO{glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)}
        },
        CornellRenderable{
            .mesh_name = "cornell_plane",
            .pipeline_name = "cornell_right",
            .model = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f)) *
                glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            .material = make_material(glm::vec3(0.14f, 0.62f, 0.18f), 8.0f)
        },
        CornellRenderable{
            .mesh_name = "cornell_cube",
            .pipeline_name = "cornell_short_box",
            .model = glm::translate(glm::mat4(1.0f), glm::vec3(-0.45f, -0.6f, -0.15f)) *
                glm::rotate(glm::mat4(1.0f), glm::radians(-18.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
                glm::scale(glm::mat4(1.0f), glm::vec3(0.6f, 0.8f, 0.6f)),
            .material = make_material(glm::vec3(0.82f, 0.82f, 0.82f), 24.0f)
        },
        CornellRenderable{
            .mesh_name = "cornell_cube",
            .pipeline_name = "cornell_tall_box",
            .model = glm::translate(glm::mat4(1.0f), glm::vec3(0.38f, -0.35f, 0.32f)) *
                glm::rotate(glm::mat4(1.0f), glm::radians(14.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
                glm::scale(glm::mat4(1.0f), glm::vec3(0.55f, 1.3f, 0.55f)),
            .material = make_material(glm::vec3(0.82f, 0.82f, 0.82f), 24.0f)
        }
    };
    renderables[3].shader_type = vkkk::built_in_shader::BuiltInShaderType::FixedColor;

    for (auto& renderable : renderables) {
        const auto shader_type = renderable.shader_type;
        if (!shader_mgr.create_pipeline(
            renderable.pipeline_name,
            shader_type,
            {vkkk::VERTEX, vkkk::NORMAL},
            ppl_opt))
        {
            throw std::runtime_error("failed to create pipeline: " + renderable.pipeline_name);
        }
    }

    vkkk::ImGuiHud hud;
    if (!hud.init(&ins)) {
        throw std::runtime_error("failed to initialize imgui hud");
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

        PhongLightUBO light{};
        light.lightPos = glm::vec4(0.0f, 0.85f, 0.0f, 1.0f);
        light.lightColor = glm::vec4(0.18f, 0.18f, 0.18f, 1.0f);
        light.viewPos = glm::vec4(cam.pos, 1.0f);

        for (const auto& renderable : renderables) {
            update_renderable_uniforms(ins, renderable, idx, light);
        }

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

        VkRenderPassBeginInfo renderpass_info{};
        renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderpass_info.renderPass = ins.get_renderpass();
        renderpass_info.framebuffer = ins.get_framebuffers()[idx];
        renderpass_info.renderArea.offset = {0, 0};
        renderpass_info.renderArea.extent = ins.get_swapchain_extent();

        std::array<VkClearValue, 2> clear_values{};
        clear_values[0].color = {{0.f, 0.f, 0.f, 1.f}};
        clear_values[1].depthStencil = {1.f, 0};
        renderpass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
        renderpass_info.pClearValues = clear_values.data();

        vkCmdBeginRenderPass(cmd, &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);
        for (const auto& renderable : renderables) {
            auto mesh_found = ins.meshes.find(renderable.mesh_name);
            if (mesh_found == ins.meshes.end()) {
                continue;
            }
            const auto pipeline_found = ins.pipelines.find(renderable.pipeline_name);
            if (pipeline_found == ins.pipelines.end()) {
                continue;
            }
            const auto& pipeline = pipeline_found->second;
            ins.bind_graphics_pipeline(cmd, pipeline.pipeline);
            mesh_found->second.emit_draw_cmd(
                cmd,
                pipeline.ppl_layout,
                &pipeline.descriptor_sets[idx]
            );
        }
        hud.render(cmd);
        vkCmdEndRenderPass(cmd);

        if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer");
        }
    });

    ins.create_sync_objects();

    using Clock = std::chrono::steady_clock;
    auto next_frame_tick = Clock::now();
    while (!glfwWindowShouldClose(ins.get_window())) {
        glfwPollEvents();

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
                // Running behind schedule; resync to avoid compounding drift.
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
    hud.shutdown();

    return 0;
}