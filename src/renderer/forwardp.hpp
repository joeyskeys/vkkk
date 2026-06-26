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
    const char* type_name() const override { return "ForwardP"; }

    bool initialize(Context* context) override;
    void shutdown() override;
    void on_resize(uint32_t width, uint32_t height) override;

    void update(const RenderView& view) override;
    void record_commands(vk::CommandBuffer cmd, const RenderView& view) override;

    template <bool transparent>
    void add_drawable(const std::string& mesh_name, const std::string& pipeline_name, const InstanceAttr& instance_attr) {
        if constexpr (transparent) {
            transparent_batch[pipeline_name][mesh_name].push_back(instance_attr);
        } else {
            opaque_batch[pipeline_name][mesh_name].push_back(instance_attr);
        }
    }

    using add_opaque_drawable = add_drawable<false>;
    using add_transparent_drawable = add_drawable<true>;

private:
    void prepare_light_clusters(const RenderView& view);
    void pass_opaque(vk::CommandBuffer cmd, const RenderView& view);
    void pass_transparent(vk::CommandBuffer cmd, const RenderView& view);
    void pass_shadow(vk::CommandBuffer cmd, const RenderView& view);
    void draw_batch(vk::CommandBuffer cmd, const RenderView& view, const Batch& batch);

    Batch opaque_batch;
    Batch transparent_batch;
};

}