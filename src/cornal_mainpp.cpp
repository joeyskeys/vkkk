#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include "asset_mgr/drawable_mgr.h"
#include "asset_mgr/light_mgr.h"
#include "asset_mgr/scene.h"
#include "built_in_shader/fixed_color.h"
#include "built_in_shader/phong.h"
#include "concepts/camera.h"
#include "vk_ins/context.hpp"
#include "vk_ins/shader_module_pack.hpp"

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

void key_callback(GLFWwindow* win, int key, int code, int action, int mods) {
    (void)win;
    (void)code;
    (void)mods;
    const bool key_down = action == GLFW_PRESS || action == GLFW_REPEAT;
    const bool key_up = action == GLFW_RELEASE;

    if (key == GLFW_KEY_E) {
        cam.y = key_down ? .01f : (key_up ? 0.f : cam.y);
    }
    else if (key == GLFW_KEY_Q) {
        cam.y = key_down ? -.01f : (key_up ? 0.f : cam.y);
    }
    else if (key == GLFW_KEY_W) {
        cam.z = key_down ? .01f : (key_up ? 0.f : cam.z);
    }
    else if (key == GLFW_KEY_S) {
        cam.z = key_down ? -.01f : (key_up ? 0.f : cam.z);
    }
    else if (key == GLFW_KEY_A) {
        cam.x = key_down ? -.01f : (key_up ? 0.f : cam.x);
    }
    else if (key == GLFW_KEY_D) {
        cam.x = key_down ? .01f : (key_up ? 0.f : cam.x);
    }
}

void mouse_btn_callback(GLFWwindow* win, int btn, int action, int mods) {
    (void)mods;
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

void mouse_pos_callback(GLFWwindow* win, double x, double y) {
    (void)win;
    if (cam.rotating) {
        const float delta_x = static_cast<float>((x - cam.prev_x) / 100.0);
        const float delta_y = static_cast<float>((y - cam.prev_y) / 100.0);
        cam.prev_x = x;
        cam.prev_y = y;
        cam.rotation = glm::angleAxis(delta_x, glm::vec3(0, 1, 0));
        cam.rotation = glm::angleAxis(-delta_y, glm::vec3(1, 0, 0)) * cam.rotation;
    }
}

using vkkk::built_in_shader::PhongLightUBO;
using vkkk::built_in_shader::PhongMaterialUBO;
using vkkk::built_in_shader::PhongTransformUBO;
using vkkk::built_in_shader::FixedColorUBO;
using vkkk::built_in_shader::FixedColorTransformUBO;

enum class CornellShaderType {
    Phong,
    FixedColor
};

struct CornellRenderable {
    CornellShaderType shader_type = CornellShaderType::Phong;
    std::string mesh_name;
    std::string pipeline_name;
    glm::mat4 model{1.0f};
    PhongMaterialUBO material{};
    FixedColorUBO fixed_color{};
};

bool build_shader_pack(vkkk::ShaderModulePack& pack, CornellShaderType type) {
    vkkk::ShaderModule vert;
    vkkk::ShaderModule frag;
    if (type == CornellShaderType::Phong) {
        if (!vert.load(vkkk::built_in_shader::phong_vert, vk::ShaderStageFlagBits::eVertex, "phong_vert")) {
            return false;
        }
        if (!frag.load(vkkk::built_in_shader::phong_frag, vk::ShaderStageFlagBits::eFragment, "phong_frag")) {
            return false;
        }
    }
    else {
        if (!vert.load(vkkk::built_in_shader::fixed_color_vert, vk::ShaderStageFlagBits::eVertex, "fixed_color_vert")) {
            return false;
        }
        if (!frag.load(vkkk::built_in_shader::fixed_color_frag, vk::ShaderStageFlagBits::eFragment, "fixed_color_frag")) {
            return false;
        }
    }
    return pack.add_shader_module(vert, true) && pack.add_shader_module(frag, true);
}

void update_renderable_uniforms(vkkk::Context& ctx, const CornellRenderable& renderable,
    uint32_t swapchain_idx, const PhongLightUBO& light_ubo)
{
    if (renderable.shader_type == CornellShaderType::Phong) {
        PhongTransformUBO transform_ubo{};
        transform_ubo.model = renderable.model;
        transform_ubo.view = cam.get_view_mat();
        transform_ubo.proj = cam.get_proj_mat();

        auto& transform = ctx.require_ubo(renderable.pipeline_name + ":ubo");
        auto& material = ctx.require_ubo(renderable.pipeline_name + ":material");
        auto& light = ctx.require_ubo(renderable.pipeline_name + ":light");

        ctx.sync_uniform(transform.memos[swapchain_idx], &transform_ubo, sizeof(transform_ubo));
        ctx.sync_uniform(material.memos[swapchain_idx], &renderable.material, sizeof(renderable.material));
        ctx.sync_uniform(light.memos[swapchain_idx], &light_ubo, sizeof(light_ubo));
    }
    else {
        FixedColorTransformUBO transform_ubo{};
        transform_ubo.model = renderable.model;
        transform_ubo.view = cam.get_view_mat();
        transform_ubo.proj = cam.get_proj_mat();

        auto& transform = ctx.require_ubo(renderable.pipeline_name + ":ubo");
        auto& fixed_color = ctx.require_ubo(renderable.pipeline_name + ":fixed_color");

        ctx.sync_uniform(transform.memos[swapchain_idx], &transform_ubo, sizeof(transform_ubo));
        ctx.sync_uniform(fixed_color.memos[swapchain_idx], &renderable.fixed_color, sizeof(renderable.fixed_color));
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

void upload_mesh(vkkk::Context& ctx, vkkk::Scene& scene, const std::string& name) {
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
    const auto glfw_extensions = vkkk::WrappedContext::get_glfw_instance_extensions();
    vkkk::Context ctx("vkkk", VK_MAKE_VERSION(1, 0, 0), "vulkan", vk::ApiVersion13, true, {}, glfw_extensions);
    GLFWwindow* window = vkkk::WrappedContext::create_window(WIDTH, HEIGHT, "Cornell Box (HPP)");
    ctx.init(window);

    vkkk::Scene scene;
    scene.drawable_mgr->add_plane("cornell_plane", {vkkk::VERTEX, vkkk::NORMAL}, 2.0f);
    scene.drawable_mgr->add_cube("cornell_cube", {vkkk::VERTEX, vkkk::NORMAL}, 1.0f);
    scene.light_mgr->add_pt_light(glm::vec4(0.0f, 0.85f, 0.0f, 1.0f), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    upload_mesh(ctx, scene, "cornell_plane");
    upload_mesh(ctx, scene, "cornell_cube");

    vkkk::PipelineOption ppl_opt;
    ppl_opt.setup_multisampling(false, vk::SampleCountFlagBits::e1);
    ppl_opt.setup_rasterizer(false, false, vk::PolygonMode::eFill, 1.0f,
        vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise, false);
    ppl_opt.setup_depth_stencil(true, true, vk::CompareOp::eLess, false, false);
    ppl_opt.setup_viewport(0.0f, 0.0f, static_cast<float>(WIDTH), static_cast<float>(HEIGHT), 0.0f, 1.0f);
    ppl_opt.setup_scissor(0, 0, WIDTH, HEIGHT);

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
    renderables[3].shader_type = CornellShaderType::FixedColor;

    const std::vector<vkkk::VERT_COMP> vertex_components{vkkk::VERTEX, vkkk::NORMAL};
    for (const auto& renderable : renderables) {
        vkkk::ShaderModulePack shader_pack;
        if (!build_shader_pack(shader_pack, renderable.shader_type)) {
            throw std::runtime_error("failed to compile shaders for pipeline: " + renderable.pipeline_name);
        }
        if (!ctx.create_pipeline(renderable.pipeline_name, shader_pack, ppl_opt, vertex_components)) {
            throw std::runtime_error("failed to create pipeline: " + renderable.pipeline_name);
        }
    }

    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_btn_callback);
    glfwSetCursorPosCallback(window, mouse_pos_callback);

    bool limit_fps_enabled = true;
    float target_fps = 60.0f;
    float current_fps = 0.0f;
    float frame_dt = 0.0f;

    ctx.set_update_cbk([&](uint32_t image_index, float dt) {
        frame_dt = dt;
        cam.update_position(frame_dt);
        cam.update_orientation();

        PhongLightUBO light{};
        light.lightPos = glm::vec4(0.0f, 0.85f, 0.0f, 1.0f);
        light.lightColor = glm::vec4(0.18f, 0.18f, 0.18f, 1.0f);
        light.viewPos = glm::vec4(cam.pos, 1.0f);

        for (const auto& renderable : renderables) {
            update_renderable_uniforms(ctx, renderable, image_index, light);
        }

        ctx.record_cmds(image_index, [&](vk::raii::CommandBuffer& cmd_buf, uint32_t idx) {
            (void)idx;
            for (const auto& renderable : renderables) {
                const auto mesh_found = ctx.meshes.find(renderable.mesh_name);
                if (mesh_found == ctx.meshes.end()) {
                    continue;
                }
                const auto pipeline_found = ctx.pipelines.find(renderable.pipeline_name);
                if (pipeline_found == ctx.pipelines.end()) {
                    continue;
                }
                const auto& pipeline = pipeline_found->second;
                cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline.vk_pipeline);
                const vk::DescriptorSet* desc_set = pipeline.descriptor_sets.empty()
                    ? nullptr
                    : &*pipeline.descriptor_sets[image_index];
                mesh_found->second.emit_draw_cmd(cmd_buf, *pipeline.vk_pipeline_layout, desc_set);
            }
        });
    });

    using Clock = std::chrono::steady_clock;
    auto next_frame_tick = Clock::now();
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        const auto frame_begin = Clock::now();
        ctx.draw_frame();
        const auto frame_end = Clock::now();

        if (limit_fps_enabled && target_fps > 1.0f) {
            const auto frame_period = std::chrono::duration<float>(1.0f / target_fps);
            next_frame_tick += std::chrono::duration_cast<Clock::duration>(frame_period);
            if (frame_end < next_frame_tick) {
                std::this_thread::sleep_until(next_frame_tick);
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
        glfwSetWindowTitle(window, ("Cornell Box (HPP) | FPS: " + std::to_string(static_cast<int>(current_fps))).c_str());
    }

    ctx.wait_idle();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
