#pragma once

#include <vector>
#include <tuple>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "renderer/renderer.hpp"
#include "vk_ins/context.hpp"
#include "vk_ins/shader_module_pack.hpp"

namespace vkkk
{

class Camera;
class Scene;

class ForwardPRenderer final : public Renderer {
public:
    ForwardPRenderer(Context* context, Scene* scene) : Renderer(context, scene) {}
    
    const char* type_name() const override { return "ForwardP"; }

    bool initialize(Context* context) override;
    void shutdown() override;
    void on_resize(uint32_t width, uint32_t height) override;

    void record_commands(vk::CommandBuffer cmd, const RenderView& view) override;

    template <bool transparent>
    void add_drawable(const std::string& mesh_name, const std::string& pipeline_name, const size_t instance_attr_size, const char* data) {
        if constexpr (transparent) {
            if (transparent_intermediate_ssbo_data.find(pipeline_name) == transparent_intermediate_ssbo_data.end()) {
                transparent_intermediate_ssbo_data[pipeline_name].first = 0;
                transparent_intermediate_ssbo_data[pipeline_name].second.clear();
            }
            auto& buffer = transparent_intermediate_ssbo_data[pipeline_name].second[mesh_name];
            buffer.insert(buffer.end(), data, data + instance_attr_size);
            transparent_intermediate_ssbo_data[pipeline_name].first += instance_attr_size;
        } else {
            if (opaque_intermediate_ssbo_data.find(pipeline_name) == opaque_intermediate_ssbo_data.end()) {
                opaque_intermediate_ssbo_data[pipeline_name].first = 0;
                opaque_intermediate_ssbo_data[pipeline_name].second.clear();
            }
            auto& buffer = opaque_intermediate_ssbo_data[pipeline_name].second[mesh_name];
            buffer.insert(buffer.end(), data, data + instance_attr_size);
            opaque_intermediate_ssbo_data[pipeline_name].first += instance_attr_size;
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
    void prepare_light_clusters(const RenderView& view);

    inline void pass_opaque(vk::CommandBuffer cmd, const RenderView& view) {
        draw_batch(cmd, view, opaque_batch);
    }

    inline void pass_transparent(vk::CommandBuffer cmd, const RenderView& view) {
        draw_batch(cmd, view, transparent_batch);
    }

    void pass_shadow(vk::CommandBuffer cmd, const RenderView& view);
    void draw_batch(vk::CommandBuffer cmd, const RenderView& view, const Batch& batch);

    IntermediateSSBOData opaque_intermediate_ssbo_data;
    IntermediateSSBOData transparent_intermediate_ssbo_data;
    Batch opaque_batch;
    Batch transparent_batch;
};

}