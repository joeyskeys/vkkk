#pragma once

#include <vector>
#include <tuple>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "renderer/renderer.hpp"
#include "vk_ins/context.hpp"
#include "vk_ins/shader_module_pack.hpp"

namespace vkkk
{

struct Camera;
class Scene;

class ForwardPRenderer final : public Renderer {
public:
    ForwardPRenderer(Context* context, Scene* scene) : Renderer(context, scene) {}
    
    const char* type_name() const override { return "ForwardP"; }

    bool initialize(Context* context) override;
    void shutdown() override;
    void on_resize(uint32_t width, uint32_t height) override;

    void record_commands(vk::CommandBuffer cmd, const uint32_t swapchain_image_idx) override;
    void prepare_light_clusters(vk::CommandBuffer cmd, uint32_t swapchain_image_idx);
    void set_max_lights_per_cluster(uint32_t count) {
        max_lights_per_cluster = count == 0 ? 1u : count;
    }

    template <bool transparent>
    void add_drawable(const std::string& mesh_name, const std::string& pipeline_name, const size_t instance_attr_size, const char* data) {
        auto add_drawable_to_batch = [&](IntermediateSSBODataMap& intermediate_ssbo_data_map) {
            if (intermediate_ssbo_data_map.find(pipeline_name) == intermediate_ssbo_data_map.end()) {
                intermediate_ssbo_data_map[pipeline_name].total_size = 0;
                intermediate_ssbo_data_map[pipeline_name].data_map[mesh_name].instance_count = 0;
                intermediate_ssbo_data_map[pipeline_name].data_map[mesh_name].data.clear();
            }
            auto& [instance_cnt, buffer] = intermediate_ssbo_data_map[pipeline_name].data_map[mesh_name];
            instance_cnt += 1;
            buffer.insert(buffer.end(), data, data + instance_attr_size);
            intermediate_ssbo_data_map[pipeline_name].total_size += instance_attr_size;
        };
        if constexpr (transparent) {
            add_drawable_to_batch(transparent_intermediate_ssbo_data_map);
        } else {
            add_drawable_to_batch(opaque_intermediate_ssbo_data_map);
        }
    }

    inline void add_opaque_drawable(const std::string& mesh_name, const std::string& pipeline_name, const size_t instance_attr_size, const char* data) {
        add_drawable<false>(mesh_name, pipeline_name, instance_attr_size, data);
	}
    inline void add_transparent_drawable(const std::string& mesh_name, const std::string& pipeline_name, const size_t instance_attr_size, const char* data) {
        add_drawable<true>(mesh_name, pipeline_name, instance_attr_size, data);
    }

    void allocate_ssbo();

private:
    inline void pass_opaque(vk::CommandBuffer cmd, const uint32_t swapchain_image_idx) {
        draw_batch(cmd, swapchain_image_idx, opaque_batches);
    }

    inline void pass_transparent(vk::CommandBuffer cmd, const uint32_t swapchain_image_idx) {
        draw_batch(cmd, swapchain_image_idx, transparent_batches);
    }

    void pass_shadow(vk::CommandBuffer cmd, const uint32_t swapchain_image_idx);
    void draw_batch(vk::CommandBuffer cmd, const uint32_t swapchain_image_idx, const Batches& batches);

    IntermediateSSBODataMap opaque_intermediate_ssbo_data_map;
    IntermediateSSBODataMap transparent_intermediate_ssbo_data_map;
    Batches opaque_batches;
    Batches transparent_batches;
    uint32_t max_lights_per_cluster = 64;
};

}