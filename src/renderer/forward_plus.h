#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "built_in_shader/phong.h"
#include "renderer/renderer.h"
#include "vk_ins/context.hpp"
#include "vk_ins/shader_module_pack.hpp"

namespace fs = std::filesystem;

namespace vkkk
{

class Camera;
class Scene;

struct ForwardPlusDrawItem {
    std::string mesh_name;
    std::string pipeline_name;
    glm::mat4 model{1.0f};
    bool transparent = false;
    built_in_shader::PhongMaterialUBO material{};
};

constexpr static std::unordered_map<std::string, const char*> built_in_shaders = {
    {"fixed_color_vert", 
    "phong",
    "pbr"
};

class ForwardPlusRenderer final : public Renderer {
public:
    const char* type_name() const override { return "Forward+"; }

    bool initialize(Context* context) override;
    void shutdown() override;

    void update(const RenderView& view) override;
    void record_commands(vk::CommandBuffer cmd, const RenderView& view) override;

    void on_resize(uint32_t width, uint32_t height) override;

private:
    bool create_pass_pipelines();
    void destroy_pass_pipelines() {}

    void update_camera_aspect();
    void update_lights_from_scene();
    void update_shadow_from_scene();
    void update_global_uniforms(uint32_t swapchain_idx);
    void sync_draw_item_uniforms(uint32_t swapchain_idx, const ForwardPlusDrawItem& item,
        const std::string& pipeline_name);

    // Forward+ light clustering placeholder (compute path pending).
    void prepare_light_clusters(const RenderView& view);

    void pass_shadow(vk::CommandBuffer cmd, const RenderView& view);
    void pass_opaque(vk::CommandBuffer cmd, const RenderView& view);
    void pass_transparent(vk::CommandBuffer cmd, const RenderView& view);

private:
    built_in_shader::PhongLightUBO light_ubo_{};
    ShadowTransformUBO shadow_transform_ubo_{};
    ShadowParamsUBO shadow_params_ubo_{};
    glm::mat4 shadow_light_view_{1.0f};
    glm::mat4 shadow_light_proj_{1.0f};
    bool opaque_pipeline_uses_shadow_ = false;
};

} // namespace vkkk
