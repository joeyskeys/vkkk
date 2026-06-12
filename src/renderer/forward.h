#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <glm/mat4x4.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "built_in_shader/built_in_shader_mgr.h"
#include "built_in_shader/phong.h"
#include "renderer/renderer.h"
#include "vk_ins/context.hpp"
#include "vk_ins/shader_module_pack.hpp"

namespace fs = std::filesystem;

namespace vkkk
{

class Camera;
class Scene;

// Shadow UBOs are temporarily disabled while forward shadow pass is under migration.
// struct ShadowTransformUBO {
//     glm::mat4 model{1.0f};
//     glm::mat4 lightView{1.0f};
//     glm::mat4 lightProj{1.0f};
// };
//
// struct ShadowParamsUBO {
//     glm::mat4 lightSpace{1.0f};
//     glm::vec4 params{0.0025f, 1.0f, 0.0f, 0.0f};
// };

struct PostParamsUBO {
    float exposure{1.0f};
    float gamma{2.2f};
    float _pad0{0.0f};
    float _pad1{0.0f};
};

struct ForwardDrawItem {
    std::string mesh_name;
    std::string pipeline_name;
    built_in_shader::BuiltInShaderType shader_type{
        built_in_shader::BuiltInShaderType::Phong
    };
    glm::mat4 model{1.0f};
    bool transparent = false;
    built_in_shader::PhongMaterialUBO material{};
};

class ForwardRenderer final : public Renderer {
public:
    static constexpr const char* kOpaquePipeline = "forward_opaque";
    static constexpr const char* kTransparentPipeline = "forward_transparent";
    static constexpr const char* kPostPipeline = "forward_post";

    // Temporary non-shadow opaque shaders (shadow-enabled pair disabled for now).
    static constexpr const char* kShaderOpaqueVert = "transparent.vert";
    static constexpr const char* kShaderOpaqueFrag = "transparent.frag";
    static constexpr const char* kShaderTransparentVert = "transparent.vert";
    static constexpr const char* kShaderTransparentFrag = "transparent.frag";
    static constexpr const char* kShaderPostVert = "post.vert";
    static constexpr const char* kShaderPostFrag = "post.frag";

    const char* type_name() const override { return "Forward"; }

    // Legacy interface entry; old wrapper path is disabled.
    bool initialize(VkWrappedInstance* instance) override;
    bool initialize(Context* context);
    void shutdown() override;

    void set_scene(Scene* scene) override;
    void set_camera(Camera* camera) { camera_ = camera; }

    void set_overlay_draw(std::function<void(vk::CommandBuffer)> draw) {
        overlay_draw_ = std::move(draw);
    }

    void add_draw_item(const ForwardDrawItem& item);
    void clear_draw_items();

    void update(const RenderView& view) override;
    void record_commands(VkCommandBuffer cmd, const RenderView& view) override;
    void record_commands(vk::CommandBuffer cmd, const RenderView& view);

    void on_resize(uint32_t width, uint32_t height) override;

    const std::vector<std::string>& missing_shaders() const { return missing_shaders_; }

private:
    bool create_pass_pipelines();
    void destroy_pass_pipelines() {}
    std::optional<fs::path> resolve_shader_path(const char* filename);
    bool load_shader_pair(const char* vert_file, const char* frag_file,
        ShaderModulePack& pack);
    bool create_shader_pipeline(const char* pipeline_name,
        const char* vert_file, const char* frag_file,
        const std::vector<VERT_COMP>& components, PipelineOption& option);

    void update_camera_aspect();
    void update_lights_from_scene();
    void update_global_uniforms(uint32_t swapchain_idx);
    void sync_draw_item_uniforms(uint32_t swapchain_idx, const ForwardDrawItem& item,
        const std::string& pipeline_name);

    void pass_opaque(vk::CommandBuffer cmd, const RenderView& view);
    void pass_transparent(vk::CommandBuffer cmd, const RenderView& view);
    void pass_post_process(vk::CommandBuffer cmd, const RenderView& view);

    void draw_batch(vk::CommandBuffer cmd, const RenderView& view,
        const std::vector<const ForwardDrawItem*>& items, const char* fallback_pipeline);

    bool ensure_draw_item_pipeline(const ForwardDrawItem& item);

    PipelineOption make_base_pipeline_option() const;
    PipelineOption make_transparent_pipeline_option() const;
    PipelineOption make_post_pipeline_option() const;

private:
    Context* ctx_ = nullptr;
    Scene* scene_ = nullptr;
    Camera* camera_ = nullptr;
    std::vector<ForwardDrawItem> draw_items_;
    std::vector<std::string> missing_shaders_;

    built_in_shader::PhongLightUBO light_ubo_{};
    // ShadowParamsUBO shadow_params_ubo_{};
    PostParamsUBO post_params_ubo_{};

    bool post_pipeline_ready_ = false;

    uint32_t width_ = 0;
    uint32_t height_ = 0;

    std::function<void(vk::CommandBuffer)> overlay_draw_;
};

} // namespace vkkk
