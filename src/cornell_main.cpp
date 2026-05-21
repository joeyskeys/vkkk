#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include "asset_mgr/scene.h"
#include "built_in_shader/built_in_shader_mgr.h"
#include "built_in_shader/phong.h"
#include "concepts/camera.h"
#include "vk_ins/cmd_buf.h"
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

vkkk::Camera cam{
    glm::vec3{0.0f, 0.0f, 3.8f},
    glm::vec3{0.0f, 0.0f, -1.0f},
    glm::vec3{0.0f, 1.0f, 0.0f},
    35.0f,
    1.333334f,
    0.1f,
    100.0f
};

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

namespace
{

using vkkk::built_in_shader::PhongLightUBO;
using vkkk::built_in_shader::PhongMaterialUBO;
using vkkk::built_in_shader::PhongTransformUBO;

struct CornellRenderable {
    std::string mesh_name;
    std::string pipeline_name;
    glm::mat4 model{1.0f};
    PhongMaterialUBO material{};
    VkPipeline pipeline{VK_NULL_HANDLE};
    VkPipelineLayout layout{VK_NULL_HANDLE};
    VkDescriptorPool descriptor_pool{VK_NULL_HANDLE};
    std::vector<VkDescriptorSet> descriptor_sets;
};

vkkk::UBO& require_ubo(vkkk::VkWrappedInstance& ins, const std::string& full_name) {
    auto found = ins.ubos.find(full_name);
    if (found == ins.ubos.end()) {
        throw std::runtime_error("ubo not found: " + full_name);
    }
    return found->second;
}

void create_descriptor_sets_for_pipeline(vkkk::VkWrappedInstance& ins, CornellRenderable& renderable) {
    const auto ppl_found = ins.pipelines.find(renderable.pipeline_name);
    if (ppl_found == ins.pipelines.end()) {
        throw std::runtime_error("pipeline not found: " + renderable.pipeline_name);
    }

    renderable.pipeline = ppl_found->second.pipeline;
    renderable.layout = ppl_found->second.ppl_layout;

    auto& transform = require_ubo(ins, renderable.pipeline_name + ":UniformBufferObject");
    auto& material = require_ubo(ins, renderable.pipeline_name + ":PhongMaterial");
    auto& light = require_ubo(ins, renderable.pipeline_name + ":PhongLight");

    const uint32_t swapchain_cnt = ins.get_swapchain_cnt();
    renderable.descriptor_sets.resize(swapchain_cnt);

    const std::array<VkDescriptorPoolSize, 1> pool_sizes{{
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = swapchain_cnt * 3
        }
    }};

    VkDescriptorPoolCreateInfo pool_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = swapchain_cnt,
        .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
        .pPoolSizes = pool_sizes.data()
    };
    if (vkCreateDescriptorPool(ins.get_device(), &pool_info, nullptr, &renderable.descriptor_pool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool for " + renderable.pipeline_name);
    }

    std::vector<VkDescriptorSetLayout> layouts(swapchain_cnt, ppl_found->second.descriptor_layout);
    VkDescriptorSetAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = renderable.descriptor_pool,
        .descriptorSetCount = swapchain_cnt,
        .pSetLayouts = layouts.data()
    };
    if (vkAllocateDescriptorSets(ins.get_device(), &alloc_info, renderable.descriptor_sets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets for " + renderable.pipeline_name);
    }

    for (uint32_t i = 0; i < swapchain_cnt; ++i) {
        VkDescriptorBufferInfo transform_info{
            .buffer = transform.gpu_bufs[i],
            .offset = 0,
            .range = transform.size * transform.vecsize
        };
        VkDescriptorBufferInfo material_info{
            .buffer = material.gpu_bufs[i],
            .offset = 0,
            .range = material.size * material.vecsize
        };
        VkDescriptorBufferInfo light_info{
            .buffer = light.gpu_bufs[i],
            .offset = 0,
            .range = light.size * light.vecsize
        };

        std::array<VkWriteDescriptorSet, 3> writes{{
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = renderable.descriptor_sets[i],
                .dstBinding = transform.binding,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &transform_info
            },
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = renderable.descriptor_sets[i],
                .dstBinding = material.binding,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &material_info
            },
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = renderable.descriptor_sets[i],
                .dstBinding = light.binding,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &light_info
            }
        }};
        vkUpdateDescriptorSets(ins.get_device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

void update_renderable_uniforms(vkkk::VkWrappedInstance& ins, const CornellRenderable& renderable,
    uint32_t swapchain_idx, const PhongLightUBO& light_ubo)
{
    PhongTransformUBO transform_ubo{};
    transform_ubo.model = renderable.model;
    transform_ubo.view = cam.get_view_mat();
    transform_ubo.proj = cam.get_proj_mat();

    auto& transform = require_ubo(ins, renderable.pipeline_name + ":UniformBufferObject");
    auto& material = require_ubo(ins, renderable.pipeline_name + ":PhongMaterial");
    auto& light = require_ubo(ins, renderable.pipeline_name + ":PhongLight");

    ins.sync_uniform(transform.memos[swapchain_idx], &transform_ubo, sizeof(transform_ubo));
    ins.sync_uniform(material.memos[swapchain_idx], &renderable.material, sizeof(renderable.material));
    ins.sync_uniform(light.memos[swapchain_idx], &light_ubo, sizeof(light_ubo));
}

PhongMaterialUBO make_material(const glm::vec3& color, float shininess = 16.0f) {
    PhongMaterialUBO material{};
    material.ambient = glm::vec4(color * 0.08f, 1.0f);
    material.diffuse = glm::vec4(color, 1.0f);
    material.specular = glm::vec4(0.18f, 0.18f, 0.18f, 1.0f);
    material.shininess = shininess;
    return material;
}

} // namespace

int main() {
    vkkk::VkWrappedInstance ins;
    ins.init_glfw();
    ins.init();
    ins.list_physical_devices();
    ins.create_resources(VK_SAMPLE_COUNT_8_BIT);

    vkkk::Scene scene;
    scene.drawable_mgr->add_plane("cornell_plane", {vkkk::VERTEX, vkkk::NORMAL}, 2.0f);
    scene.drawable_mgr->add_cube("cornell_cube", {vkkk::VERTEX, vkkk::NORMAL}, 1.0f);
    scene.drawable_mgr->upload_gpu(&ins, "cornell_plane");
    scene.drawable_mgr->upload_gpu(&ins, "cornell_cube");

    // Keep scene-level light resources alongside drawables for future extensibility.
    scene.light_mgr->add_pt_light(glm::vec4(0.0f, 0.85f, 0.0f, 1.0f), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    vkkk::built_in_shader::BuiltInShaderMgr shader_mgr(&ins);
    vkkk::PipelineOption ppl_opt;
    ppl_opt.setup_multisampling(true, ins.nsample);
    ppl_opt.setup_rasterizer(false, false, VK_POLYGON_MODE_FILL, 1.0f, VK_CULL_MODE_NONE,
        VK_FRONT_FACE_COUNTER_CLOCKWISE, false);

    std::vector<CornellRenderable> renderables{
        CornellRenderable{
            .mesh_name = "cornell_plane",
            .pipeline_name = "cornell_floor",
            .model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            .material = make_material(glm::vec3(0.78f, 0.78f, 0.78f), 8.0f)
        },
        CornellRenderable{
            .mesh_name = "cornell_plane",
            .pipeline_name = "cornell_ceiling",
            .model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
                glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            .material = make_material(glm::vec3(0.78f, 0.78f, 0.78f), 8.0f)
        },
        CornellRenderable{
            .mesh_name = "cornell_plane",
            .pipeline_name = "cornell_back",
            .model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -1.0f)) *
                glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            .material = make_material(glm::vec3(0.78f, 0.78f, 0.78f), 8.0f)
        },
        CornellRenderable{
            .mesh_name = "cornell_plane",
            .pipeline_name = "cornell_left",
            .model = glm::translate(glm::mat4(1.0f), glm::vec3(-1.0f, 0.0f, 0.0f)) *
                glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            .material = make_material(glm::vec3(0.72f, 0.12f, 0.12f), 8.0f)
        },
        CornellRenderable{
            .mesh_name = "cornell_plane",
            .pipeline_name = "cornell_right",
            .model = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f)) *
                glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            .material = make_material(glm::vec3(0.14f, 0.62f, 0.18f), 8.0f)
        },
        CornellRenderable{
            .mesh_name = "cornell_cube",
            .pipeline_name = "cornell_short_box",
            .model = glm::translate(glm::mat4(1.0f), glm::vec3(-0.45f, -0.6f, -0.15f)) *
                glm::rotate(glm::mat4(1.0f), glm::radians(-18.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
                glm::scale(glm::mat4(1.0f), glm::vec3(0.6f, 0.8f, 0.6f)),
            .material = make_material(glm::vec3(0.82f, 0.82f, 0.82f), 24.0f)
        },
        CornellRenderable{
            .mesh_name = "cornell_cube",
            .pipeline_name = "cornell_tall_box",
            .model = glm::translate(glm::mat4(1.0f), glm::vec3(0.38f, -0.35f, 0.32f)) *
                glm::rotate(glm::mat4(1.0f), glm::radians(14.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
                glm::scale(glm::mat4(1.0f), glm::vec3(0.55f, 1.3f, 0.55f)),
            .material = make_material(glm::vec3(0.82f, 0.82f, 0.82f), 24.0f)
        }
    };

    for (auto& renderable : renderables) {
        if (!shader_mgr.create_pipeline(
            renderable.pipeline_name,
            vkkk::built_in_shader::BuiltInShaderType::Phong,
            {vkkk::VERTEX, vkkk::NORMAL},
            ppl_opt))
        {
            throw std::runtime_error("failed to create pipeline: " + renderable.pipeline_name);
        }
        create_descriptor_sets_for_pipeline(ins, renderable);
    }

    ins.setup_key_cbk(key_callback);
    ins.setup_mouse_btn_cbk(mouse_btn_callback);
    ins.setup_mouse_pos_cbk(mouse_pos_callback);

    vkkk::CommandBuffers cmd_bufs(&ins);
    cmd_bufs.alloc();

    ins.set_update_cbk([&](uint32_t idx, float duration) {
        cam.update_position(duration);
        cam.update_orientation();

        PhongLightUBO light{};
        light.lightPos = glm::vec4(0.0f, 0.85f, 0.0f, 1.0f);
        light.lightColor = glm::vec4(18.0f, 18.0f, 18.0f, 1.0f);
        light.viewPos = glm::vec4(cam.pos, 1.0f);

        for (const auto& renderable : renderables) {
            update_renderable_uniforms(ins, renderable, idx, light);
        }
    });

    ins.record_cmds(
        cmd_bufs.bufs,
        ins.get_framebuffers(),
        [&](uint32_t idx) {
            for (const auto& renderable : renderables) {
                auto mesh_found = ins.meshes.find(renderable.mesh_name);
                if (mesh_found == ins.meshes.end()) {
                    continue;
                }
                ins.bind_graphics_pipeline(cmd_bufs[idx], renderable.pipeline);
                mesh_found->second.emit_draw_cmd(
                    cmd_bufs[idx],
                    renderable.layout,
                    &renderable.descriptor_sets[idx]
                );
            }
        }
    );

    ins.create_sync_objects();
    ins.mainloop(cmd_bufs);
    for (auto& renderable : renderables) {
        if (renderable.descriptor_pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(ins.get_device(), renderable.descriptor_pool, nullptr);
            renderable.descriptor_pool = VK_NULL_HANDLE;
        }
    }

    return 0;
}