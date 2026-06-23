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

class ForwardPlusRenderer final : public Renderer {
public:
    static constexpr const char* kOpaquePipeline = "forward_plus_opaque";
    static constexpr const char* kTransparentPipeline = "forward_plus_transparent";

    static constexpr const char* kShaderOpaqueVert = "transparent.vert";
    static constexpr const char* kShaderOpaqueFrag = "transparent.frag";
    static constexpr const char* kShaderTransparentVert = "transparent.vert";
    static constexpr const char* kShaderTransparentFrag = "transparent.frag";

    const char* type_name() const override { return "Forward+"; }

    bool initialize(Context* context) override;
    void shutdown() override;

    void set_overlay_draw(std::function<void(vk::CommandBuffer)> draw) {
        overlay_draw_ = std::move(draw);
    }

    void add_draw_item(const ForwardPlusDrawItem& item);
    void clear_draw_items();

    void update(const RenderView& view) override;
    void record_commands(vk::CommandBuffer cmd, const RenderView& view) override;

    void on_resize(uint32_t width, uint32_t height) override;
    const std::vector<std::string>& missing_shaders() const { return missing_shaders_; }

private:
    bool create_pass_pipelines();
    void destroy_pass_pipelines() {}
    std::optional<fs::path> resolve_shader_path(const char* filename);
    bool load_shader_pair(const char* vert_file, const char* frag_file, ShaderModulePack& pack);
    bool create_shader_pipeline(const char* pipeline_name,
        const char* vert_file, const char* frag_file,
        const std::vector<VERT_COMP>& components, PipelineOption& option);

    void update_camera_aspect();
    void update_lights_from_scene();
    void update_shadow_from_scene();
    void update_global_uniforms(uint32_t swapchain_idx);
    void sync_draw_item_uniforms(uint32_t swapchain_idx, const ForwardPlusDrawItem& item,
        const std::string& pipeline_name);

    // Forward+ light clustering placeholder (compute path pending).
    void prepare_light_clusters(const RenderView& view);

    void pass_opaque(vk::CommandBuffer cmd, const RenderView& view);
    void pass_transparent(vk::CommandBuffer cmd, const RenderView& view);
    void pass_shadow(vk::CommandBuffer cmd, const RenderView& view) {(void)cmd; (void)view;}
    void draw_batch(vk::CommandBuffer cmd, const RenderView& view,
        const std::vector<const ForwardPlusDrawItem*>& items, const char* fallback_pipeline);
    bool ensure_draw_item_pipeline(const ForwardPlusDrawItem& item);
    PipelineOption make_base_pipeline_option() const;
    PipelineOption make_transparent_pipeline_option() const;

private:
    std::vector<ForwardPlusDrawItem> draw_items_;
    std::vector<std::string> missing_shaders_;
    vkkk::PhongLightUBO light_ubo_{};
    //ShadowTransformUBO shadow_transform_ubo_{};
    //ShadowParamsUBO shadow_params_ubo_{};
    glm::mat4 shadow_light_view_{1.0f};
    glm::mat4 shadow_light_proj_{1.0f};
    bool opaque_pipeline_uses_shadow_ = false;
    std::function<void(vk::CommandBuffer)> overlay_draw_;
};

} // namespace vkkk
