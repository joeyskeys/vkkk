#include <iostream>
#include <array>

#include <GLFW/glfw3.h>

#include "concepts/camera.h"
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

vkkk::Camera cam{glm::vec3{0, 0, 5}, glm::vec3{0, 0, -1}, glm::vec3{0, 1, 0}, 35, 1.333334f, 1, 100};

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
    ins.init_glfw();
    ins.init();
    ins.list_physical_devices();
    ins.create_resources(VK_SAMPLE_COUNT_8_BIT);

    std::vector<vkkk::ShaderModule> modules(2);
    if (!modules[0].load("../resource/shaders/default.vert", VK_SHADER_STAGE_VERTEX_BIT))
        throw std::runtime_error("failed to load vertex shader");
    if (!modules[1].load("../resource/shaders/default.frag", VK_SHADER_STAGE_FRAGMENT_BIT))
        throw std::runtime_error("failed to load fragment shader");

    vkkk::PipelineOption ppl_opt;
    ppl_opt.setup_multisampling(true, ins.nsample);
    if (!ins.create_pipeline("default", modules, {}, ppl_opt))
        throw std::runtime_error("failed to create default pipeline");

    const float tri_vertices[] = {
        0.0f, -0.5f, 0.0f,
        0.5f,  0.5f, 0.0f,
       -0.5f,  0.5f, 0.0f
    };
    const uint32_t tri_indices[] = {0, 1, 2};
    vkkk::Mesh tri_mesh_cpu({vkkk::VERTEX});
    tri_mesh_cpu.load(3, reinterpret_cast<const char*>(tri_vertices), sizeof(tri_vertices),
        1, reinterpret_cast<const char*>(tri_indices), sizeof(tri_indices));
    if (!ins.load_mesh("tri", tri_mesh_cpu))
        throw std::runtime_error("failed to upload triangle mesh");

    ins.setup_key_cbk(key_callback);
    ins.setup_mouse_btn_cbk(mouse_btn_callback);
    ins.setup_mouse_pos_cbk(mouse_pos_callback);

    vkkk::CommandBuffers cmd_bufs(&ins);
    cmd_bufs.alloc();

    auto found = ins.pipelines.find("default");
    if (found == ins.pipelines.end())
        throw std::runtime_error("pipeline default not found");

    auto& vk_pipeline = found->second.pipeline;
    auto ppl_layout = found->second.ppl_layout;
    auto mesh_found = ins.meshes.find("tri");
    if (mesh_found == ins.meshes.end())
        throw std::runtime_error("triangle mesh not found");
    auto& tri_mesh_gpu = mesh_found->second;

    auto cam_ubo_found = ins.ubos.find("default:camera");
    if (cam_ubo_found == ins.ubos.end())
        cam_ubo_found = ins.ubos.find("default:Camera");
    if (cam_ubo_found == ins.ubos.end())
        throw std::runtime_error("camera ubo not found");
    auto& cam_ubo = cam_ubo_found->second;
    auto swapchain_cnt = ins.get_swapchain_cnt();
    std::vector<vkkk::CameraGPU> cam_gpus(swapchain_cnt);
    for (uint32_t i = 0; i < swapchain_cnt; ++i) {
        auto& cam_gpu = cam_gpus[i];
        cam_gpu.binding = cam_ubo.binding;
        cam_gpu.buf = cam_ubo.gpu_bufs[i];
        cam_gpu.memo = cam_ubo.memos[i];
        cam_gpu.descriptor.buffer = cam_gpu.buf;
        cam_gpu.descriptor.offset = 0;
        cam_gpu.descriptor.range = cam_ubo.size * cam_ubo.vecsize;
    }

    VkDescriptorPoolSize pool_size{
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = swapchain_cnt
    };
    VkDescriptorPoolCreateInfo pool_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
        .maxSets = swapchain_cnt
    };
    VkDescriptorPool cam_desc_pool;
    if (vkCreateDescriptorPool(ins.get_device(), &pool_info, nullptr, &cam_desc_pool) != VK_SUCCESS)
        throw std::runtime_error("failed to create camera descriptor pool");

    std::vector<VkDescriptorSetLayout> cam_layouts(swapchain_cnt, found->second.descriptor_layout);
    std::vector<VkDescriptorSet> cam_desc_sets(swapchain_cnt);
    VkDescriptorSetAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = cam_desc_pool,
        .descriptorSetCount = swapchain_cnt,
        .pSetLayouts = cam_layouts.data()
    };
    if (vkAllocateDescriptorSets(ins.get_device(), &alloc_info, cam_desc_sets.data()) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate camera descriptor sets");

    for (uint32_t i = 0; i < swapchain_cnt; ++i) {
        VkWriteDescriptorSet write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = cam_desc_sets[i],
            .dstBinding = cam_gpus[i].binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &cam_gpus[i].descriptor
        };
        vkUpdateDescriptorSets(ins.get_device(), 1, &write, 0, nullptr);
    }

    ins.set_update_cbk([&](uint32_t idx, float duration) {
        cam.update_position(duration);
        cam.update_orientation();
        cam_gpus[idx].sync(cam, &ins);
    });

    ins.record_cmds(
        cmd_bufs.bufs,
        ins.get_framebuffers(),
        [&](uint32_t idx) {
            auto& cmd = cmd_bufs[idx];
            ins.bind_graphics_pipeline(cmd, vk_pipeline);
            tri_mesh_gpu.emit_draw_cmd(cmd, ppl_layout, &cam_desc_sets[idx]);
        }
    );

    ins.create_sync_objects();
    ins.mainloop(cmd_bufs);
    vkDestroyDescriptorPool(ins.get_device(), cam_desc_pool, nullptr);

    return 0;
}