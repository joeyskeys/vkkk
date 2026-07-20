#include <chrono>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include "asset_mgr/drawable_mgr.h"
#include "asset_mgr/scene.h"
#include "built_in_shader/line_gen.h"
#include "concepts/camera.h"
#include "gui/gui.h"
#include "renderer/forwardp.hpp"
#include "vk_ins/context.hpp"

namespace
{

constexpr uint32_t width = 800;
constexpr uint32_t height = 600;
constexpr char plane_pipeline_name[] = "wireframe_plane_mat";
constexpr char cube_pipeline_name[] = "wireframe_cube_mat";

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

struct WireInstance {
    const char* mesh_name;
    glm::mat4 model;
    glm::vec4 color;
};

} // namespace

int main() {
    vkkk::Context ctx;
    GLFWwindow* window = ctx.init_glfw(width, height, "Wireframe Cornell (Mesh Shader)");
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

    const auto create_wireframe_pipeline = [&](const char* name) {
        return renderer.create_pipeline_from_shader_src(
            name,
            vkkk::gen_line_task,
            vkkk::gen_line_mesh,
            vkkk::gen_line_frag,
            make_pipeline_option(),
            {});
    };
    if (!create_wireframe_pipeline(plane_pipeline_name)
        || !create_wireframe_pipeline(cube_pipeline_name))
    {
        throw std::runtime_error("failed to create wireframe mesh pipeline");
    }
    if (!ctx.bind_pipeline_ssbo_from_mesh(plane_pipeline_name, "cornell_plane")
        || !ctx.bind_pipeline_ssbo_from_mesh(cube_pipeline_name, "cornell_cube"))
    {
        throw std::runtime_error("failed to bind wireframe mesh buffers");
    }

    const WireInstance instances[] = {
        {
            "cornell_plane",
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f))
                * glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            glm::vec4(0.85f, 0.85f, 0.85f, 1.0f)
        },
        {
            "cornell_plane",
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -1.0f))
                * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            glm::vec4(0.85f, 0.85f, 0.85f, 1.0f)
        },
        {
            "cornell_plane",
            glm::translate(glm::mat4(1.0f), glm::vec3(-1.0f, 0.0f, 0.0f))
                * glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            glm::vec4(0.90f, 0.25f, 0.25f, 1.0f)
        },
        {
            "cornell_plane",
            glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f))
                * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            glm::vec4(0.25f, 0.80f, 0.30f, 1.0f)
        },
        {
            "cornell_plane",
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            glm::vec4(0.85f, 0.85f, 0.85f, 1.0f)
        },
        {
            "cornell_cube",
            glm::translate(glm::mat4(1.0f), glm::vec3(-0.45f, -0.6f, -0.15f))
                * glm::rotate(glm::mat4(1.0f), glm::radians(-18.0f), glm::vec3(0.0f, 1.0f, 0.0f))
                * glm::scale(glm::mat4(1.0f), glm::vec3(0.6f, 0.8f, 0.6f)),
            glm::vec4(0.95f, 0.95f, 0.95f, 1.0f)
        },
        {
            "cornell_cube",
            glm::translate(glm::mat4(1.0f), glm::vec3(0.38f, -0.35f, 0.32f))
                * glm::rotate(glm::mat4(1.0f), glm::radians(14.0f), glm::vec3(0.0f, 1.0f, 0.0f))
                * glm::scale(glm::mat4(1.0f), glm::vec3(0.55f, 1.3f, 0.55f)),
            glm::vec4(0.95f, 0.95f, 0.95f, 1.0f)
        },
    };

    std::vector<vkkk::LineGenParamsUBO> plane_instances;
    std::vector<vkkk::LineGenParamsUBO> cube_instances;
    for (const auto& instance : instances) {
        vkkk::LineGenParamsUBO params{};
        params.model = instance.model;
        params.color = instance.color;
        if (std::string_view(instance.mesh_name) == "cornell_plane") {
            plane_instances.push_back(params);
        } else {
            cube_instances.push_back(params);
        }
    }

    const auto allocate_instance_ssbo = [&](const char* name, size_t instance_count) {
        return ctx.resize_pipeline_ssbo(name, vkkk::SSBOType_LineGenParams, instance_count)
            && ctx.alloc_pipeline_ssbo(name, vkkk::SSBOType_LineGenParams);
    };
    if (!allocate_instance_ssbo(plane_pipeline_name, plane_instances.size())
        || !allocate_instance_ssbo(cube_pipeline_name, cube_instances.size()))
    {
        throw std::runtime_error("failed to allocate wireframe instance attributes");
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
        ImGui::Text("Renderer: Wireframe Mesh Shader");
        ImGui::Checkbox("Limit FPS", &limit_fps_enabled);
        ImGui::SliderFloat("Target FPS", &target_fps, 15.0f, 240.0f, "%.0f");
        ImGui::Text("FPS: %.1f", current_fps);
        ImGui::Text("Frame: %.2f ms", frame_dt * 1000.0f);
        ImGui::Text("Raw dt: %.2f ms", raw_frame_dt * 1000.0f);
        ImGui::Text("Plane instances: %d", 5);
        ImGui::Text("Box instances: %d", 2);
        ImGui::End();

        ctx.record_cmds(image_index, [&](vk::raii::CommandBuffer& cmd, uint32_t swapchain_index) {
            const auto draw_instances = [&](const char* pipeline_name, const char* mesh_name,
                const std::vector<vkkk::LineGenParamsUBO>& instance_params)
            {
                const auto pipeline_it = ctx.pipelines.find(pipeline_name);
                const auto mesh_it = ctx.meshes.find(mesh_name);
                if (pipeline_it == ctx.pipelines.end() || mesh_it == ctx.meshes.end()
                    || instance_params.empty())
                {
                    return;
                }

                const auto& pipeline = pipeline_it->second;
                const auto camera_ubo_it = pipeline.ubos.find(vkkk::UBOType_Camera);
                const auto mesh_info_ubo_it = pipeline.ubos.find(vkkk::UBOType_LineGenMeshInfo);
                const auto instance_ssbo_it = pipeline.ssbos.find(vkkk::SSBOType_LineGenParams);
                if (camera_ubo_it == pipeline.ubos.end()
                    || mesh_info_ubo_it == pipeline.ubos.end()
                    || instance_ssbo_it == pipeline.ssbos.end()
                    || swapchain_index >= camera_ubo_it->second.memos.size()
                    || swapchain_index >= mesh_info_ubo_it->second.memos.size())
                {
                    return;
                }

                const uint32_t index_count = mesh_it->second.icnt * 3u;
                vkkk::LineGenMeshInfoUBO mesh_info{};
                mesh_info.vertex_stride_floats = 6;
                mesh_info.index_count = index_count;
                mesh_info.instance_count = static_cast<uint32_t>(instance_params.size());
                ctx.sync_uniform(
                    camera_ubo_it->second.memos[swapchain_index],
                    &camera.ubo_data,
                    static_cast<uint32_t>(sizeof(camera.ubo_data)));
                ctx.sync_uniform(
                    mesh_info_ubo_it->second.memos[swapchain_index],
                    &mesh_info,
                    static_cast<uint32_t>(sizeof(mesh_info)));
                ctx.sync_ssbo(
                    instance_ssbo_it->second,
                    instance_params.data(),
                    swapchain_index,
                    static_cast<uint32_t>(instance_params.size() * sizeof(vkkk::LineGenParamsUBO)));

                const uint32_t triangles_per_instance = index_count / 3u;
                const uint32_t task_groups_per_instance =
                    (triangles_per_instance + vkkk::line_gen_triangles_per_task - 1u)
                    / vkkk::line_gen_triangles_per_task;
                if (!ctx.record_mesh_tasks(
                        cmd,
                        pipeline_name,
                        task_groups_per_instance * mesh_info.instance_count,
                        1,
                        1,
                        swapchain_index))
                {
                    throw std::runtime_error("failed to record mesh tasks for wireframe draw");
                }
            };

            draw_instances(plane_pipeline_name, "cornell_plane", plane_instances);
            draw_instances(cube_pipeline_name, "cornell_cube", cube_instances);

            hud.render(static_cast<VkCommandBuffer>(*cmd));
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
