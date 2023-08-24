#include <iostream>

#include <GLFW/glfw3.h>

#include "asset_mgr/light_mgr.h"
#include "asset_mgr/mesh_mgr.h"
#include "concepts/camera.h"
#include "concepts/lights.h"
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

vkkk::CameraDeprecated cam{glm::vec3{0, 0, 5}, glm::vec3{0, 0, -1}, glm::vec3{0, 1, 0}, 35, 1.333334f, 1, 100};

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
        cam.rotation = glm::angleAxis(delta_y, glm::vec3(1, 0, 0)) * cam.rotation;
    }
}

int main() {
    vkkk::VkWrappedInstance ins;
    ins.init_glfw();
    ins.init();
    ins.list_physical_devices();
    ins.create_resources(VK_SAMPLE_COUNT_8_BIT);

    //vkkk::PipelineMgr pipeline_mgr(&ins);
    auto& pipeline_mgr = vkkk::PipelineMgr::instance(&ins);
    pipeline_mgr.register_pipeline("object");
    pipeline_mgr.register_pipeline("skybox");
    pipeline_mgr.register_pipeline("forward");
    pipeline_mgr.register_pipeline("matte");
    auto& pipeline_obj = pipeline_mgr.get_pipeline("object");
    auto& pipeline_sky = pipeline_mgr.get_pipeline("skybox");
    auto& pipeline_for = pipeline_mgr.get_pipeline("forward");
    auto& pipeline_mat = pipeline_mgr.get_pipeline("matte");

    pipeline_sky.depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    pipeline_sky.rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    pipeline_sky.multisampling.sampleShadingEnable = VK_TRUE;
    // pipeline rasterization sample should be unified or the result will
    // be wrong
    pipeline_sky.multisampling.rasterizationSamples = ins.nsample;
    pipeline_for.multisampling.sampleShadingEnable = VK_TRUE;
    pipeline_for.multisampling.rasterizationSamples = ins.nsample;
    pipeline_obj.multisampling.sampleShadingEnable = VK_TRUE;
    pipeline_obj.multisampling.rasterizationSamples = ins.nsample;
    pipeline_mat.multisampling.sampleShadingEnable = VK_TRUE;
    pipeline_mat.multisampling.rasterizationSamples = ins.nsample;

    pipeline_obj.modules->add_module("../resource/shaders/with_tex_vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    pipeline_obj.modules->add_module("../resource/shaders/with_tex_frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    pipeline_obj.modules->assign_tex_image("tex_sampler", "../resource/textures/8k_moon.jpg");
    pipeline_obj.modules->alloc_uniforms();

    pipeline_sky.modules->add_module("../resource/shaders/skybox_vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    pipeline_sky.modules->add_module("../resource/shaders/skybox_frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    pipeline_sky.modules->assign_tex_image("cubemap_spl", "../resource/textures/skybox1.png", true);
    pipeline_sky.modules->alloc_uniforms();

    pipeline_for.modules->add_module("../resource/shaders/basic_lighting_vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    pipeline_for.modules->add_module("../resource/shaders/basic_lighting_frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    pipeline_for.modules->alloc_uniforms();

    pipeline_mat.modules->add_module("../resource/shaders/matte_vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    pipeline_mat.modules->add_module("../resource/shaders/matte_frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    pipeline_mat.modules->alloc_uniforms();

    // We need a concept of a complete scene now..
    auto& light_mgr = vkkk::LightMgr::instance();
    light_mgr.add_pt_light(glm::vec4(0, 10, 10, 1), glm::vec4(1, 1, 0, 1));
    light_mgr.add_dir_light(glm::vec4(1, 0, 0, 0), glm::vec4(0.5, 0.1, 0.1, 1));
    light_mgr.add_spot_light(glm::vec4(0, 4, 0, 1), glm::vec4(0, -1, 0, 0), glm::vec3(0, 0, 0.7), 5.f);

    auto update_cbk = [&](uint32_t idx, float duration) {
        cam.update_position(duration);
        cam.update_orientation();

        auto obj_ubo_ptr = pipeline_obj.uniforms->find_ubo("ubo");
        if (!obj_ubo_ptr)
            return;
        auto obj_buf = reinterpret_cast<glm::mat4*>(obj_ubo_ptr->cpu_buf.get());
        obj_buf[0] = glm::translate(glm::mat4(1), glm::vec3(3, 0, 0));
        obj_buf[1] = cam.get_view_mat();
        obj_buf[2] = cam.get_proj_mat();
        pipeline_obj.uniforms->update_ubos(idx);

        auto sky_ubo_ptr = pipeline_sky.uniforms->find_ubo("ubo");
        if (!sky_ubo_ptr)
            return;
        auto sky_buf = reinterpret_cast<glm::mat4*>(sky_ubo_ptr->cpu_buf.get());
        sky_buf[0] = glm::mat4(1);
        sky_buf[1] = cam.get_view_mat();
        sky_buf[1][3] = glm::vec4(0.f, 0.f, 0.f, 1.f);
        sky_buf[2] = cam.get_proj_mat();
        pipeline_sky.uniforms->update_ubos(idx);

        auto xforms_ptr = pipeline_for.uniforms->find_ubo("xforms");
        if (!xforms_ptr)
            return;
        auto xforms_buf = reinterpret_cast<glm::mat4*>(xforms_ptr->cpu_buf.get());
        xforms_buf[0] = cam.get_proj_mat() * cam.get_view_mat();

        auto light_info_ptr = pipeline_for.uniforms->find_ubo("infos");
        if (!light_info_ptr)
            return;
        auto light_infos_buf = reinterpret_cast<void*>(light_info_ptr->cpu_buf.get());
        light_mgr.update_uniform(light_infos_buf);
        pipeline_for.uniforms->update_ubos(idx);

        auto mat_ubo_ptr = pipeline_mat.uniforms->find_ubo("ubo");
        if (!mat_ubo_ptr)
            return;
        auto mat_buf = reinterpret_cast<glm::mat4*>(mat_ubo_ptr->cpu_buf.get());
        mat_buf[0] = glm::mat4(1);
        mat_buf[1] = cam.get_view_mat();
        mat_buf[2] = cam.get_proj_mat();

        auto mat_color_ptr = pipeline_mat.uniforms->find_ubo("datas");
        if (!mat_color_ptr)
            return;
        auto color_buf = reinterpret_cast<glm::vec3*>(mat_color_ptr->cpu_buf.get());
        *color_buf = glm::vec3(0.4, 0.6, 0.6);
        pipeline_mat.uniforms->update_ubos(idx);
    };

    ins.set_update_cbk(update_cbk);
    ins.setup_key_cbk(key_callback);
    ins.setup_mouse_btn_cbk(mouse_btn_callback);
    ins.setup_mouse_pos_cbk(mouse_pos_callback);

    pipeline_mgr.create_descriptor_layouts();

    pipeline_obj.modules->set_attribute_binding(0, 0);
    pipeline_obj.modules->set_attribute_binding(0, 1);
    pipeline_obj.modules->create_input_descriptions({vkkk::VERTEX, vkkk::UV});

    pipeline_sky.modules->set_attribute_binding(0, 0);
    pipeline_sky.modules->create_input_descriptions({vkkk::VERTEX, vkkk::UV});

    pipeline_for.modules->set_attribute_binding(0, 0);
    pipeline_for.modules->set_attribute_binding(0, 1);
    pipeline_for.modules->set_attribute_binding(0, 2);
    pipeline_for.modules->create_input_descriptions({vkkk::VERTEX, vkkk::NORMAL, vkkk::UV});

    pipeline_mat.modules->set_attribute_binding(0, 0);
    pipeline_mat.modules->set_attribute_binding(0, 1);
    pipeline_mat.modules->set_attribute_binding(0, 2);
    pipeline_mat.modules->create_input_descriptions({vkkk::VERTEX, vkkk::NORMAL, vkkk::UV});

    pipeline_mgr.create_pipelines();

    pipeline_mgr.create_descriptor_pools();
    pipeline_mgr.create_descriptor_sets();
    
    auto& mesh_mgr = vkkk::MeshMgr::instance(&ins);
    //mesh_mgr.init(&ins);
    mesh_mgr.load_file("../resource/models/moon.obj", {vkkk::VERTEX, vkkk::UV});
    mesh_mgr.load_file("../resource/models/skybox.obj", {vkkk::VERTEX, vkkk::UV});
    mesh_mgr.load_file("../resource/models/sphere.obj", {vkkk::VERTEX, vkkk::NORMAL, vkkk::UV});
    auto moon_obj = &mesh_mgr.meshes[0];
    auto skybox_obj = &mesh_mgr.meshes[1];
    auto sphere_obj = &mesh_mgr.meshes[2];
    mesh_mgr.pour_into_gpu();

    vkkk::CommandBuffers cmd_bufs(&ins);
    cmd_bufs.alloc();
    
    auto [obj_vk_ppl, obj_ppl_layout] = pipeline_mgr.get_vkpipeline_and_layout("object");
    auto [box_vk_ppl, box_ppl_layout] = pipeline_mgr.get_vkpipeline_and_layout("skybox");
    auto [for_vk_ppl, for_ppl_layout] = pipeline_mgr.get_vkpipeline_and_layout("forward");
    auto [mat_vk_ppl, mat_ppl_layout] = pipeline_mgr.get_vkpipeline_and_layout("matte");
    assert(obj_vk_ppl != nullptr);
    assert(box_vk_ppl != nullptr);

    ins.record_cmds(
        cmd_bufs.bufs,
        ins.get_framebuffers(),
        [&](uint32_t idx) {
            pipeline_mgr.bind("skybox", cmd_bufs, idx);
            skybox_obj->emit_draw_cmd(cmd_bufs.bufs[idx], box_ppl_layout,
                pipeline_sky.modules->get_descriptor_set(idx));
            pipeline_mgr.bind("object", cmd_bufs, idx);
            moon_obj->emit_draw_cmd(cmd_bufs.bufs[idx], obj_ppl_layout,
                pipeline_obj.modules->get_descriptor_set(idx));
            pipeline_mgr.bind("forward", cmd_bufs, idx);
            sphere_obj->emit_draw_cmd(cmd_bufs.bufs[idx], for_ppl_layout,
                pipeline_for.modules->get_descriptor_set(idx));
            /*
            pipeline_mgr.bind("matte", cmd_bufs.bufs[idx]);
            sphere_obj->emit_draw_cmd(cmd_bufs.bufs[idx], mat_ppl_layout,
                pipeline_mat.modules.get_descriptor_set(idx));
            */
        }
    );
    ins.create_sync_objects();

    ins.mainloop(cmd_bufs);

    // GPU resources need to be explicitly freed since the object
    // dtor calling sequence is not fixed when relying on RAII,
    // instance may get freed before calling other resources' dtor.
    // Guess it's same issue for the pipeline.
    mesh_mgr.free_gpu_resources();
    pipeline_mgr.free_gpu_resources();

    return 0;
}