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

Camera cam{glm::vec3{0, 0, -5}, glm::vec3{0, 0, -1}, glm::vec3{0, 1, 0}, 35, 1, 0.1, 100};

void key_callback(GLFWwindow* win, int key, int code, int action, int mods) {
    if (key == GLFW_KEY_E && action == GLFW_PRESS) {
        cam.pos.z += 0.2;
    }
    else if (key == GLFW_KEY_S && action == GLFW_PRESS) {
        cam.pos.z -= 0.2;
    }
    else if (key == GLFW_KEY_A && action == GLFW_PRESS) {
        cam.pos.x -= 0.2;
    }
    else if (key == GLFW_KEY_D && action == GLFW_PRESS) {
        cam.pos.x += 0.2;
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
    ins.create_swapchain();
    ins.create_imageviews();
    ins.create_renderpass();
    ins.create_descriptor_set_layout();
    //ins.create_graphics_pipeline();

    vkkk::ShaderModules modules(ins.get_device());
    modules.add_module("../resource/shaders/depth_default_vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    modules.add_module("../resource/shaders/depth_default_frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    ins.create_graphics_pipeline(modules, vkkk::ONLY_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_POLYGON_MODE_LINE);

    ins.create_depth_resource();
    ins.create_framebuffers();
    ins.create_command_pool();

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
    
    auto mesh_mgr = vkkk::MeshMgr::instance();
    mesh_mgr.load_file("../resource/model/box.obj", vkkk::ONLY_VERTEX);
    const auto& mesh = mesh_mgr.meshes[0];
    ins.create_vertex_buffer(mesh.vbuf.get(), mesh.comp_size, mesh.vcnt);
    ins.create_index_buffer(mesh.ibuf.get(), mesh.icnt);

    ins.create_uniform_buffer();
    ins.set_uniform_cbk(ubo_update);
    ins.create_descriptor_pool();
    ins.create_descriptor_set();

    ins.create_commandbuffers();
    ins.create_sync_objects();

    ins.mainloop();

    return 0;
}