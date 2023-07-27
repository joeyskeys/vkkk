#include <iostream>

#include <GLFW/glfw3.h>

#include "asset_mgr/mesh_mgr.h"
#include "concepts/camera.h"
#include "vk_ins/cmd_buf.h"
#include "vk_ins/vkabstraction.h"
#include "vk_ins/pipeline_mgr.h"

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

Camera cam{glm::vec3{0, 0, 5}, glm::vec3{0, 0, -1}, glm::vec3{0, 1, 0}, 35, 1.333334f, 1, 100};

void key_callback(GLFWwindow* win, int key, int code, int action, int mods) {
    if (key == GLFW_KEY_E) {
        if (action == GLFW_PRESS)
            cam.y = 1.f;
        else
            cam.y = 0.f;
    }
    else if (key == GLFW_KEY_Q) {
        if (action == GLFW_PRESS)
            cam.y = -1.f;
        else
            cam.y = 0.f;
    }
    else if (key == GLFW_KEY_W) {
        if (action == GLFW_PRESS)
            cam.z = 1.f;
        else
            cam.z = 0.f;
    }
    else if (key == GLFW_KEY_S) {
        if (action == GLFW_PRESS)
            cam.z = -1.f;
        else
            cam.z = 0.f;
    }
    else if (key == GLFW_KEY_A) {
        if (action == GLFW_PRESS)
            cam.x = -1.f;
        else
            cam.x = 0.f;
    }
    else if (key == GLFW_KEY_D) {
        if (action == GLFW_PRESS)
            cam.x = 1.f;
        else
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

int main() {
    vkkk::VkWrappedInstance ins;
    ins.create_surface();
    ins.create_logical_device();
    auto swapchain_img_cnt = ins.create_swapchain();
    ins.create_imageviews();
    ins.create_renderpass();
    ins.create_command_pool();

    vkkk::PipelineMgr pipeline_mgr(&ins);
    pipeline_mgr.register_pipeline("object");
    pipeline_mgr.register_pipeline("skybox");
    auto& pipeline_obj = pipeline_mgr.get_pipeline("object");
    auto& pipeline_sky = pipeline_mgr.get_pipeline("skybox");

    pipeline_sky.depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    pipeline_obj.modules.add_module("../resource/shaders/with_tex_vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    pipeline_obj.modules.add_module("../resource/shaders/with_tex_frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    pipeline_obj.modules.assign_tex_image("tex_sampler", "../resource/textures/8k_moon.jpg");
    pipeline_obj.modules.alloc_uniforms();

    pipeline_sky.modules.add_module("../resource/shaders/skybox_vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    pipeline_sky.modules.add_module("../resource/shaders/skybox_frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    pipeline_sky.modules.assign_tex_image("cubemap_spl", "../resource/textures/skybox1.png", true);
    pipeline_sky.modules.alloc_uniforms();

    auto update_cbk = [&](uint32_t idx, float duration) {
        cam.update_position(duration);
        cam.update_orientation();

        auto obj_ubo_ptr = pipeline_obj.uniforms->find_ubo("ubo");
        if (!obj_ubo_ptr)
            return;
        auto obj_buf = reinterpret_cast<vkkk::MVPBuffer*>(obj_ubo_ptr->cpu_buf.get());
        obj_buf->model = glm::mat4(1);
        obj_buf->view = cam.get_view_mat();
        obj_buf->proj = cam.get_proj_mat();
        pipeline_obj.uniforms->update_ubos(idx);

        auto sky_ubo_ptr = pipeline_sky.uniforms->find_ubo("ubo");
        if (!sky_ubo_ptr)
            return;
        auto sky_buf = reinterpret_cast<vkkk::MVPBuffer*>(sky_ubo_ptr->cpu_buf.get());
        sky_buf->model = glm::mat4(1);
        sky_buf->view = cam.get_view_mat();
        sky_buf->view[3] = glm::vec4(0.f, 0.f, 0.f, 1.f);
        sky_buf->proj = cam.get_proj_mat();
        pipeline_sky.uniforms->update_ubos(idx);
    };

    ins.set_update_cbk(update_cbk);
    ins.setup_key_cbk(key_callback);
    ins.setup_mouse_btn_cbk(mouse_btn_callback);
    ins.setup_mouse_pos_cbk(mouse_pos_callback);

    pipeline_mgr.create_descriptor_layouts();

    pipeline_obj.modules.set_attribute_binding(0, 0);
    pipeline_obj.modules.set_attribute_binding(0, 1);
    pipeline_sky.modules.set_attribute_binding(0, 0);
    pipeline_mgr.create_input_descriptions({vkkk::VERTEX, vkkk::UV});

    pipeline_mgr.create_pipelines(ins.get_renderpass());

    ins.create_depth_resource();
    ins.create_framebuffers();

    pipeline_mgr.create_descriptor_pools();
    pipeline_mgr.create_descriptor_sets();
    
    auto mesh_mgr = vkkk::MeshMgr::instance();
    mesh_mgr.init(&ins);
    mesh_mgr.load_file("../resource/models/moon.obj", {vkkk::VERTEX, vkkk::UV});
    mesh_mgr.load_file("../resource/models/skybox.obj", {vkkk::VERTEX, vkkk::UV});
    auto moon_obj = &mesh_mgr.meshes[0];
    auto skybox_obj = &mesh_mgr.meshes[1];
    mesh_mgr.pour_into_gpu();

    vkkk::CommandBuffers cmd_bufs(&ins);
    cmd_bufs.alloc();
    
    auto [obj_vk_ppl, obj_ppl_layout] = pipeline_mgr.get_vkpipeline_and_layout("object");
    auto [box_vk_ppl, box_ppl_layout] = pipeline_mgr.get_vkpipeline_and_layout("skybox");
    assert(obj_vk_ppl != nullptr);
    assert(box_vk_ppl != nullptr);

    ins.record_cmds(
        cmd_bufs.bufs,
        ins.get_framebuffers(),
        [&](uint32_t idx) {
            pipeline_mgr.bind("skybox", cmd_bufs.bufs[idx]);
            skybox_obj->emit_draw_cmd(cmd_bufs.bufs[idx], box_ppl_layout,
                pipeline_sky.modules.get_descriptor_set(idx));
            pipeline_mgr.bind("object", cmd_bufs.bufs[idx]);
            moon_obj->emit_draw_cmd(cmd_bufs.bufs[idx], obj_ppl_layout,
                pipeline_obj.modules.get_descriptor_set(idx));
        }
    );
    ins.create_sync_objects();

    ins.mainloop(cmd_bufs);

    return 0;
}