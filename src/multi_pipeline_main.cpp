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

bool check_validation_layer_support() {
    uint32_t layer_cnt;
    vkEnumerateInstanceLayerProperties(&layer_cnt, nullptr);

    std::vector<VkLayerProperties> availableLayers(layer_cnt);
    vkEnumerateInstanceLayerProperties(&layer_cnt, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

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

    pipeline_obj.modules.add_module("../resource/shaders/with_tex_vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    pipeline_obj.modules.add_module("../resource/shaders/with_tex_frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    pipeline_obj.modules.assign_tex_image("tex_sampler", "../resource/textures/8k_moon.jpg");
    pipeline_obj.modules.alloc_uniforms();

    pipeline_sky.modules.add_module("../resource/shaders/skybox_vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    pipeline_sky.modules.add_module("../resource/shaders/skybox_frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    pipeline_sky.modules.assign_tex_image("cubemap_spl", "../resource/textures/skybox1.png");
    pipeline_sky.modules.alloc_uniforms();

    auto update_cbk = [&](uint32_t idx, float duration) {
        cam.update_position(duration);
        cam.update_orientation();
        /*
        auto ubo_ptr = uniform_mgr.find_ubo("ubo");
        if (!ubo_ptr)
            return;
        auto buf = reinterpret_cast<vkkk::MVPBuffer*>(ubo_ptr->cpu_buf.get());
        buf->model = glm::mat4(1);
        buf->view = cam.get_view_mat();
        buf->proj = cam.get_proj_mat();
        uniform_mgr.update_ubos(idx);
        */
    };

    ins.set_update_cbk(update_cbk);
    ins.setup_key_cbk(key_callback);
    ins.setup_mouse_btn_cbk(mouse_btn_callback);
    ins.setup_mouse_pos_cbk(mouse_pos_callback);

    pipeline_mgr.create_descriptor_layouts();

    pipeline_obj.modules.set_attribute_binding(0, 0);
    pipeline_obj.modules.set_attribute_binding(0, 1);
    pipeline_sky.modules.set_attribute_binding(0, 0);
    pipeline_mgr.create_input_descriptions();

    pipeline_mgr.create_pipelines(ins.get_renderpass());

    ins.create_depth_resource();
    ins.create_framebuffers();

    pipeline_mgr.create_descriptor_pools();
    pipeline_mgr.create_descriptor_sets();
    
    auto mesh_mgr = vkkk::MeshMgr::instance();
    auto moon_obj = mesh_mgr.load_file("../resource/models/moon.obj", {vkkk::VERTEX, vkkk::UV});
    auto skybox_obj = mesh_mgr.load_file("../resource/models/skybox.obj", {vkkk::VERTEX, vkkk::UV});
    mesh_mgr.pour_into_gpu();

    //ins.create_commandbuffers();
    vkkk::CommandBuffers cmd_bufs(&ins);
    cmd_bufs.alloc();
    
    auto [obj_vk_ppl, obj_ppl_layout] = pipeline_mgr.get_vkpipeline_and_layout("object");
    auto [box_vk_ppl, box_ppl_layout] = pipeline_mgr.get_vkpipeline_and_layout("skybox");
    assert(obj_vk_ppl != nullptr);
    ins.record_cmds(
        obj_vk_ppl,
        cmd_bufs.bufs,
        ins.get_framebuffers(),
        [&]() {
            for (int i = 0; i < ins.get_swapchain_cnt(); ++i) {
                moon_obj->emit_draw_cmd(cmd_bufs.bufs[i], obj_ppl_layout,
                    pipeline_obj.modules.get_descriptor_set(i));
                skybox_obj->emit_draw_cmd(cmd_bufs.bufs[i], box_ppl_layout,
                    pipeline_sky.modules.get_descriptor_set(i));
            }
        }
    );
    ins.create_sync_objects();

    ins.mainloop();

    return 0;
}