#include <iostream>
#include <array>

#include <GLFW/glfw3.h>

#include "gui/gui.h"
#include "asset_mgr/mesh_mgr.h"
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

void ubo_update(vkkk::MVPBuffer *buf) {
    buf->model = glm::mat4(1);
    buf->view = cam.get_view_mat();
    buf->proj = cam.get_proj_mat();
}

int main() {
    vkkk::VkWrappedInstance ins;
    ins.create_surface();
    ins.create_logical_device();
    auto swapchain_img_cnt = ins.create_swapchain();
    ins.create_imageviews();
    ins.create_renderpass();
    //ins.create_descriptor_set_layout();
    //ins.create_graphics_pipeline();

    vkkk::UniformMgr uniform_mgr{ &ins };
    vkkk::ShaderModules modules{ &ins, &uniform_mgr };
    modules.add_module("../resource/shaders/depth_no_tex_vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    modules.add_module("../resource/shaders/depth_no_tex_frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    //modules.add_module("../resource/shaders/default_vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    //modules.add_module("../resource/shaders/default_frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    modules.alloc_uniforms(std::unordered_map<std::string, std::string>());

    /*
    ins.load_texture("../resource/textures/texture.jpeg");
    ins.create_texture_imageviews();
    ins.create_texture_sampler();
    */
    
    /*
    const std::array<vkkk::VertexTmp, 8> verts{
        glm::vec3{-0.5f, -0.5f, 0.f}, glm::vec3{1.f, 0.f, 0.f}, glm::vec2{1.f, 0.f},
        glm::vec3{0.5f, -0.5f, 0.f}, glm::vec3{0.f, 1.f, 0.f}, glm::vec2{0.f, 0.f},
        glm::vec3{0.5f, 0.5f, 0.f}, glm::vec3{0.f, 0.f, 1.f}, glm::vec2{0.f, 1.f},
        glm::vec3{-0.5f, 0.5f, 0.f}, glm::vec3{1.f, 1.f, 1.f}, glm::vec2{1.f, 1.f},

        glm::vec3{-0.5f, -0.5f, -0.5f}, glm::vec3{1.f, 0.f, 0.f}, glm::vec2{1.f, 0.f},
        glm::vec3{0.5f, -0.5f, -0.5f}, glm::vec3{0.f, 1.f, 0.f}, glm::vec2{0.f, 0.f},
        glm::vec3{0.5f, 0.5f, -0.5f}, glm::vec3{0.f, 0.f, 1.f}, glm::vec2{0.f, 1.f},
        glm::vec3{-0.5f, 0.5f, -0.5f}, glm::vec3{1.f, 1.f, 1.f}, glm::vec2{1.f, 1.f}
    };
    ins.create_vertex_buffer(verts.data(), verts.size() * sizeof(vkkk::Vertex));

    const std::array<uint32_t, 12> indices{
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4
    };
    ins.create_index_buffer(indices.data(), indices.size());
    */

    auto update_cbk = [&](uint32_t idx, float duration) {
        cam.update_position(duration);
        auto ubo_ptr = uniform_mgr.find_ubo("UniformBufferObject");
        if (!ubo_ptr)
            return;
        auto buf = reinterpret_cast<vkkk::MVPBuffer*>(ubo_ptr->cpu_buf.get());
        buf->model = glm::mat4(1);
        buf->view = cam.get_view_mat();
        buf->proj = cam.get_proj_mat();
        uniform_mgr.update_ubos(idx);
    };
    ins.set_update_cbk(update_cbk);

    //modules.create_descriptor_pool_and_sets();
    modules.create_descriptor_layouts();

    ins.create_graphics_pipeline(modules, vkkk::ONLY_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_POLYGON_MODE_FILL);

    modules.create_descriptor_pool();
    modules.create_descriptor_set();
    /*
    ins.create_descriptor_pool();
    ins.uniform_buffers.resize(ins.get_swapchain_cnt());
    for (int i = ins.get_swapchain_cnt(); i > 0; --i) {
        ins.uniform_buffers[i] = uniform_mgr.ubos.at(std::string("UniformBufferObject")).gpu_bufs[i];
    }
    ins.create_descriptor_set();
    */

    ins.create_depth_resource();
    ins.create_framebuffers();
    ins.create_command_pool();
    
    auto mesh_mgr = vkkk::MeshMgr::instance();
    mesh_mgr.load_file("../resource/models/sphere.obj", vkkk::ONLY_VERTEX);
    const auto& mesh = mesh_mgr.meshes[0];
    ins.create_vertex_buffer(mesh.vbuf.get(), mesh.comp_size, mesh.vcnt);
    ins.create_index_buffer(mesh.ibuf.get(), mesh.icnt * 3);

    //ins.create_uniform_buffer();
    //ins.set_uniform_cbk(ubo_update);
    //ins.create_descriptor_pool();
    //ins.create_descriptor_set();

    //ins.create_commandbuffers();
    ins.create_commandbuffers(swapchain_img_cnt, modules, mesh_mgr);
    ins.create_sync_objects();

    ins.setup_key_cbk(key_callback);

    ins.mainloop();

    return 0;
}