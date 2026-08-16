#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <GLFW/glfw3.h>

#include "asset_mgr/drawable_mgr.h"
#include "asset_mgr/scene.h"
#include "built_in_shader/phong.h"
#include "concepts/camera.h"
#include "font/font.hpp"
#include "vk_ins/shader_module_pack.hpp"
#include "vp/frame_axis.hpp"
#include "vp/grid.hpp"
#include "vp/viewport.hpp"

namespace
{

constexpr uint32_t kWidth = 1200;
constexpr uint32_t kHeight = 800;
constexpr const char* kCubeObjectName = "viewport_center_cube_object";
constexpr const char* kCubeMeshName = "viewport_center_cube";
constexpr const char* kCubePhongPipeline = "viewport_cube_phong";
constexpr const char* kCubeWirePipeline = "viewport_cube_wire";

class SceneCubeFeature final
    : public vkkk::vp::ViewportFeature<vkkk::vp::ViewportPhase::Scene> {
public:
    explicit SceneCubeFeature(vkkk::Scene& scene)
        : scene(scene)
    {
    }

    void on_attach(vkkk::Context& context, vk::Extent2D) {
        if (scene.camera == nullptr || !create_pipelines(context)) {
            return;
        }
        ready = context.resize_pipeline_ssbo(
                    kCubePhongPipeline, vkkk::buf::PhongInstanceAttrs, 1)
            && context.alloc_pipeline_ssbo(kCubePhongPipeline, vkkk::buf::PhongInstanceAttrs)
            && context.resize_pipeline_ssbo(
                kCubeWirePipeline, vkkk::buf::PhongInstanceAttrs, 1)
            && context.alloc_pipeline_ssbo(kCubeWirePipeline, vkkk::buf::PhongInstanceAttrs);
    }

    void on_update(vkkk::Context& context, const vkkk::Context::Frame&) {
        const bool c_down = glfwGetKey(context.get_window(), GLFW_KEY_C) == GLFW_PRESS;
        if (c_down && !c_was_down) {
            if (scene.find_object(kCubeObjectName) != nullptr) {
                scene.remove_object(kCubeObjectName);
            }
            else {
                scene.add_object(kCubeObjectName, kCubeMeshName);
            }
        }
        c_was_down = c_down;
    }

    void on_record(vkkk::Context& context, vk::raii::CommandBuffer& cmd, uint32_t image_index) {
        const auto* cube = scene.find_object(kCubeObjectName);
        if (!ready || cube == nullptr || scene.camera == nullptr) {
            return;
        }

        auto shaded = shaded_attrs;
        shaded.model = cube->model;
        sync_draw_data(context, kCubePhongPipeline, image_index, shaded);
        if (context.bind(cmd, kCubePhongPipeline, image_index)) {
            context.draw(cmd, kCubePhongPipeline, cube->mesh_name, 1);
        }

        auto wire = wire_attrs;
        wire.model = cube->model;
        sync_draw_data(context, kCubeWirePipeline, image_index, wire);
        if (context.bind(cmd, kCubeWirePipeline, image_index)) {
            context.draw(cmd, kCubeWirePipeline, cube->mesh_name, 1);
        }
    }

private:
    static bool create_pipeline(vkkk::Context& context, const char* pipeline_name,
        vk::PolygonMode polygon_mode, bool depth_write)
    {
        if (context.pipelines.contains(pipeline_name)) {
            return true;
        }

        vkkk::ShaderModule vert_module;
        vkkk::ShaderModule frag_module;
        if (!vert_module.load(vkkk::phong_vert, vk::ShaderStageFlagBits::eVertex,
                "viewport_cube_phong_vert")
            || !frag_module.load(vkkk::phong_frag, vk::ShaderStageFlagBits::eFragment,
                "viewport_cube_phong_frag"))
        {
            return false;
        }
        vkkk::ShaderModulePack pack;
        if (!pack.add_shader_module(vert_module) || !pack.add_shader_module(frag_module)) {
            return false;
        }

        vkkk::PipelineOption option;
        option.setup_input_assembly(vk::PrimitiveTopology::eTriangleList, false);
        option.setup_multisampling(false, vk::SampleCountFlagBits::e1);
        option.setup_rasterizer(false, false, polygon_mode, 1.0f,
            vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise, false);
        option.setup_depth_stencil(true, depth_write, vk::CompareOp::eLessOrEqual, false, false);
        return context.create_pipeline(
            pipeline_name, pack, option, {vkkk::VERTEX, vkkk::NORMAL});
    }

    void sync_draw_data(vkkk::Context& context, const char* pipeline_name,
        uint32_t image_index, const vkkk::PhongInstanceAttrs& attrs)
    {
        vkkk::PointLightUBO light{};
        light.vec = glm::vec4{2.0f, 3.0f, 2.0f, 1.0f};
        light.color = glm::vec4{1.0f};
        context.sync_ubo(
            pipeline_name, vkkk::buf::CameraUBO, &scene.camera->ubo_data, image_index);
        context.sync_ubo(pipeline_name, vkkk::buf::PointLightUBO, &light, image_index);
        context.sync_ssbo(pipeline_name, vkkk::buf::PhongInstanceAttrs, &attrs, image_index);
    }

    bool create_pipelines(vkkk::Context& context) {
        return create_pipeline(context, kCubePhongPipeline, vk::PolygonMode::eFill, true)
            && create_pipeline(context, kCubeWirePipeline, vk::PolygonMode::eLine, false);
    }

    vkkk::Scene& scene;
    vkkk::PhongInstanceAttrs shaded_attrs{
        .model = glm::mat4{1.0f},
        .ambient = glm::vec4{0.05f, 0.08f, 0.14f, 1.0f},
        .diffuse = glm::vec4{0.25f, 0.55f, 0.9f, 1.0f},
        .specular = glm::vec4{0.7f, 0.7f, 0.7f, 1.0f},
        .shininess = 32.0f,
    };
    vkkk::PhongInstanceAttrs wire_attrs{
        .model = glm::mat4{1.0f},
        .ambient = glm::vec4{0.02f, 0.02f, 0.02f, 1.0f},
        .diffuse = glm::vec4{0.03f, 0.03f, 0.03f, 1.0f},
        .specular = glm::vec4{0.0f},
        .shininess = 1.0f,
    };
    bool ready = false;
    bool c_was_down = false;
};

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
    vkkk::Scene scene;
    scene.camera = &camera;
    scene.drawable_mgr->add_cube(kCubeMeshName, {vkkk::VERTEX, vkkk::NORMAL}, 1.0f);
    scene.drawable_mgr->sync_to_gpu(&ctx);
    scene.add_object(kCubeObjectName, kCubeMeshName);

    ViewportControls viewport_controls(camera);
    controls = &viewport_controls;
    glfwSetScrollCallback(window, scroll_callback);

    using BasicViewport = vkkk::vp::Viewport<
        SceneCubeFeature,
        vkkk::vp::GridFeature,
        vkkk::vp::FrameAxisFeature,
        BillboardTextFeature>;
    BasicViewport viewport(ctx);
    const std::filesystem::path bundled_font_path =
        std::filesystem::path{VKKK_SOURCE_DIR} / "resource/font/Roboto-Light.ttf";
    // An explicit path can still override the bundled font.
    const std::filesystem::path font_path =
        argc > 1 ? std::filesystem::path{argv[1]} : bundled_font_path;
    viewport.add_feature<SceneCubeFeature>(scene);
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
