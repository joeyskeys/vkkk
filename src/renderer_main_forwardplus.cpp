#include <stdexcept>
#include <iterator>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include "asset_mgr/drawable_mgr.h"
#include "asset_mgr/light_mgr.h"
#include "asset_mgr/scene.h"
#include "built_in_shader/phong_plus.h"
#include "concepts/camera.h"
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
    renderer.set_max_lights_per_cluster(64);

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
    if (!ctx.resize_pipeline_ssbo(pipeline_name, vkkk::SSBOType_InstanceAttrs, std::size(attrs))
        || !ctx.alloc_pipeline_ssbo(pipeline_name, vkkk::SSBOType_InstanceAttrs))
    {
        throw std::runtime_error("failed to allocate instance attributes");
    }

    vkkk::PipelineLightStorage lights;
    vkkk::PointLightUBO point_light{};
    point_light.vec = glm::vec4(0.0f, 0.85f, 0.0f, 1.0f);
    point_light.color = glm::vec4(1.0f);
    point_light.radius = 5.0f;
    lights.pt_lights.push_back(point_light);
    scene.light_mgr->register_pipeline(pipeline_name, lights);

    ctx.set_update_cbk([&](uint32_t image_index, float /*dt*/) {
        camera.update_ubo_data();
        ctx.record_cmds(image_index,
            [&](vk::raii::CommandBuffer& cmd, uint32_t swapchain_index) {
                renderer.record_commands(cmd, swapchain_index);
            },
            [&](vk::raii::CommandBuffer& cmd, uint32_t swapchain_index) {
                renderer.prepare_light_clusters(cmd, swapchain_index);
            });
    });

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ctx.draw_frame();
    }

    ctx.wait_idle();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
