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

#include "asset_mgr/drawable_mgr.h"
#include "asset_mgr/scene.h"
#include "built_in_shader/phong.h"
#include "concepts/camera.h"
#include "gui/gui.h"
#include "vk_ins/context.hpp"

namespace
{

constexpr unsigned int WIDTH = 800;
constexpr unsigned int HEIGHT = 600;
constexpr uint32_t kMaxInstances = 16;

vkkk::Camera cam{
    glm::vec3{0.0f, 0.0f, 3.8f},
    glm::vec3{0.0f, 0.0f, -1.0f},
    glm::vec3{0.0f, 1.0f, 0.0f},
    35.0f,
    1.333334f,
    0.1f,
    100.0f
};

struct InstanceAttr {
    glm::mat4 model{1.0f};
};

struct InstancedPhongBatch {
    std::string mesh_name;
    std::string pipeline_name;
    vkkk::built_in_shader::PhongMaterialUBO material{};
    std::vector<InstanceAttr> instances;
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

vkkk::built_in_shader::PhongMaterialUBO make_material(const glm::vec3& color, float shininess = 16.0f) {
    vkkk::built_in_shader::PhongMaterialUBO material{};
    material.ambient = glm::vec4(color * 0.08f, 1.0f);
    material.diffuse = glm::vec4(color, 1.0f);
    material.specular = glm::vec4(0.18f, 0.18f, 0.18f, 1.0f);
    material.shininess = shininess;
    return material;
}

void upload_mesh(vkkk::Context& ctx, const vkkk::Scene& scene, const std::string& name) {
    const vkkk::Mesh* mesh = scene.drawable_mgr->find_mesh(name);
    if (mesh == nullptr) throw std::runtime_error("mesh not found: " + name);
    if (!ctx.load_mesh(name, *mesh)) throw std::runtime_error("failed to upload mesh: " + name);
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

bool create_instanced_phong_pipeline(vkkk::Context& ctx, const std::string& pipeline_name) {
    vkkk::ShaderModule vert;
    vkkk::ShaderModule frag;
    if (!vert.load(vkkk::built_in_shader::phong_vert_instanced, vk::ShaderStageFlagBits::eVertex, "phong_vert_instanced")) return false;
    if (!frag.load(vkkk::built_in_shader::phong_frag, vk::ShaderStageFlagBits::eFragment, "phong_frag")) return false;

    vkkk::ShaderModulePack pack;
    if (!pack.add_shader_module(vert, true) || !pack.add_shader_module(frag, true)) return false;
    const std::vector<vkkk::VERT_COMP> components{vkkk::VERTEX, vkkk::NORMAL};
    auto option = make_pipeline_option();
    return ctx.create_pipeline(pipeline_name, pack, option, components);
}

void sync_batch_uniforms(vkkk::Context& ctx, const InstancedPhongBatch& batch,
    uint32_t image_index, const vkkk::built_in_shader::PhongLightUBO& light_ubo)
{
    vkkk::built_in_shader::PhongTransformUBO transform{};
    transform.model = glm::mat4(1.0f);
    transform.view = cam.get_view_mat();
    transform.proj = cam.get_proj_mat();

    auto& transform_ubo = ctx.require_ubo(batch.pipeline_name + ":ubo");
    auto& material_ubo = ctx.require_ubo(batch.pipeline_name + ":material");
    auto& light_ubo_mem = ctx.require_ubo(batch.pipeline_name + ":light");
    auto& instance_attrs = ctx.require_ubo(batch.pipeline_name + ":instance_attrs");

    ctx.sync_uniform(transform_ubo.memos[image_index], &transform, static_cast<uint32_t>(sizeof(transform)));
    ctx.sync_uniform(material_ubo.memos[image_index], &batch.material, static_cast<uint32_t>(sizeof(batch.material)));
    ctx.sync_uniform(light_ubo_mem.memos[image_index], &light_ubo, static_cast<uint32_t>(sizeof(light_ubo)));

    const uint32_t max_upload = static_cast<uint32_t>(instance_attrs.size * instance_attrs.vecsize);
    const uint32_t requested = static_cast<uint32_t>(batch.instances.size() * sizeof(InstanceAttr));
    const uint32_t upload_size = std::min(max_upload, requested);
    ctx.sync_uniform(instance_attrs.memos[image_index], batch.instances.data(), upload_size);
}

} // namespace

int main() {
    vkkk::Context ctx;
    GLFWwindow* window = ctx.init_glfw(WIDTH, HEIGHT, "Forward+ Cornell (Instanced)");
    const auto glfw_extensions = vkkk::Context::get_glfw_instance_extensions();
    ctx.init(window, "vkkk", VK_MAKE_VERSION(1, 0, 0), "vulkan", vk::ApiVersion13, true, {}, glfw_extensions);

    vkkk::Scene scene;
    scene.drawable_mgr->add_plane("cornell_plane", {vkkk::VERTEX, vkkk::NORMAL}, 2.0f);
    scene.drawable_mgr->add_cube("cornell_cube", {vkkk::VERTEX, vkkk::NORMAL}, 1.0f);
    upload_mesh(ctx, scene, "cornell_plane");
    upload_mesh(ctx, scene, "cornell_cube");

    std::vector<InstancedPhongBatch> batches;
    batches.push_back({
        "cornell_plane",
        "cornell_plane_gray",
        make_material(glm::vec3(0.78f, 0.78f, 0.78f), 8.0f),
        {
            {glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, 0.0f))},
            {glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f)) * glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f))},
            {glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -1.0f)) * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f))}
        }
    });
    batches.push_back({
        "cornell_plane",
        "cornell_plane_red",
        make_material(glm::vec3(0.72f, 0.12f, 0.12f), 8.0f),
        {
            {glm::translate(glm::mat4(1.0f), glm::vec3(-1.0f, 0.0f, 0.0f)) * glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f))}
        }
    });
    batches.push_back({
        "cornell_plane",
        "cornell_plane_green",
        make_material(glm::vec3(0.14f, 0.62f, 0.18f), 8.0f),
        {
            {glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f))}
        }
    });
    batches.push_back({
        "cornell_cube",
        "cornell_boxes",
        make_material(glm::vec3(0.82f, 0.82f, 0.82f), 24.0f),
        {
            {glm::translate(glm::mat4(1.0f), glm::vec3(-0.45f, -0.6f, -0.15f)) *
                glm::rotate(glm::mat4(1.0f), glm::radians(-18.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
                glm::scale(glm::mat4(1.0f), glm::vec3(0.6f, 0.8f, 0.6f))},
            {glm::translate(glm::mat4(1.0f), glm::vec3(0.38f, -0.35f, 0.32f)) *
                glm::rotate(glm::mat4(1.0f), glm::radians(14.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
                glm::scale(glm::mat4(1.0f), glm::vec3(0.55f, 1.3f, 0.55f))}
        }
    });

    for (const auto& batch : batches) {
        if (batch.instances.empty() || batch.instances.size() > kMaxInstances) {
            throw std::runtime_error("invalid instance count in batch: " + batch.pipeline_name);
        }
        if (!create_instanced_phong_pipeline(ctx, batch.pipeline_name)) {
            throw std::runtime_error("failed to create pipeline: " + batch.pipeline_name);
        }
    }

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

        vkkk::built_in_shader::PhongLightUBO light_ubo{};
        light_ubo.lightPos = glm::vec4(0.0f, 0.85f, 0.0f, 1.0f);
        light_ubo.lightColor = glm::vec4(1.0f);
        light_ubo.viewPos = glm::vec4(cam.pos, 1.0f);

        for (const auto& batch : batches) {
            sync_batch_uniforms(ctx, batch, idx, light_ubo);
        }

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

        ctx.record_cmds(idx, [&](vk::raii::CommandBuffer& cmd_buf, uint32_t image_index) {
            for (const auto& batch : batches) {
                const auto pipeline_it = ctx.pipelines.find(batch.pipeline_name);
                if (pipeline_it == ctx.pipelines.end()) continue;
                const auto& pipeline = pipeline_it->second;
                cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline.vk_pipeline);

                const vk::DescriptorSet* desc_set = nullptr;
                if (image_index < pipeline.descriptor_sets.size()) {
                    desc_set = &*pipeline.descriptor_sets[image_index];
                }
                ctx.draw_mesh_instanced(
                    static_cast<vk::CommandBuffer>(*cmd_buf),
                    batch.mesh_name,
                    *pipeline.vk_pipeline_layout,
                    static_cast<uint32_t>(batch.instances.size()),
                    desc_set);
            }
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
