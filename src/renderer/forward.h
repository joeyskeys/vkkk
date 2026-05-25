#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <glm/mat4x4.hpp>
#include <vulkan/vulkan.h>

#include "built_in_shader/built_in_shader_mgr.h"
#include "built_in_shader/phong.h"
#include "renderer/renderer.h"

namespace fs = std::filesystem;

namespace vkkk
{

class Camera;
class Scene;

struct ShadowTransformUBO {
    glm::mat4 model{1.0f};
    glm::mat4 lightView{1.0f};
    glm::mat4 lightProj{1.0f};
};

struct ShadowParamsUBO {
    glm::mat4 lightSpace{1.0f};
    glm::vec4 params{0.0025f, 1.0f, 0.0f, 0.0f}; // bias, pcf radius
};

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
    static constexpr const char* kShadowPipeline = "forward_shadow";
    static constexpr const char* kOpaquePipeline = "forward_opaque";
    static constexpr const char* kTransparentPipeline = "forward_transparent";
    static constexpr const char* kPostPipeline = "forward_post";
    static constexpr const char* kShadowMapTarget = "forward_shadow_map";

    static constexpr const char* kShaderShadowDepthVert = "shadow_depth.vert";
    static constexpr const char* kShaderShadowDepthFrag = "shadow_depth.frag";
    static constexpr const char* kShaderOpaqueShadowVert = "opaque_shadow.vert";
    static constexpr const char* kShaderOpaqueShadowFrag = "opaque_shadow.frag";
    static constexpr const char* kShaderPostVert = "post.vert";
    static constexpr const char* kShaderPostFrag = "post.frag";

    const char* type_name() const override { return "Forward"; }

    bool initialize(VkWrappedInstance* instance) override;
    void shutdown() override;

    void set_scene(Scene* scene) override;
    void set_camera(Camera* camera) { camera_ = camera; }

    void add_draw_item(const ForwardDrawItem& item);
    void clear_draw_items();

    void render(const RenderView& view) override;
    void record_commands(VkCommandBuffer cmd, const RenderView& view);

    void on_resize(uint32_t width, uint32_t height) override;

    const std::vector<std::string>& missing_shaders() const { return missing_shaders_; }

private:
    bool create_pass_pipelines();
    void destroy_pass_pipelines();
    bool ensure_shadow_resources();
    bool create_shadow_pass_objects();
    void destroy_shadow_pass_objects();
    std::optional<fs::path> resolve_shader_path(const char* filename);
    bool load_shader_pair(const char* vert_file, const char* frag_file,
        std::vector<ShaderModule>& modules);
    bool create_shader_pipeline(const char* pipeline_name,
        const char* vert_file, const char* frag_file,
        const std::vector<VERT_COMP>& components, PipelineOption& option,
        VkRenderPass render_pass_override = VK_NULL_HANDLE);
    bool bind_shadow_map_texture(const char* pipeline_name);
    bool bind_scene_color_texture(const char* pipeline_name);

    void update_camera_aspect();
    void update_lights_from_scene();
    void update_global_uniforms(uint32_t swapchain_idx);
    void sync_draw_item_uniforms(uint32_t swapchain_idx, const ForwardDrawItem& item,
        const std::string& pipeline_name);
    void refresh_shadow_map_descriptors(uint32_t swapchain_idx);

    glm::mat4 compute_light_space_matrix() const;

    void pass_shadow_map(VkCommandBuffer cmd, const RenderView& view);
    void pass_opaque(VkCommandBuffer cmd, const RenderView& view);
    void pass_transparent(VkCommandBuffer cmd, const RenderView& view);
    void pass_post_process(VkCommandBuffer cmd, const RenderView& view);

    void draw_batch(VkCommandBuffer cmd, const RenderView& view,
        const std::vector<const ForwardDrawItem*>& items, const char* fallback_pipeline);

    bool ensure_draw_item_pipeline(const ForwardDrawItem& item);

    PipelineOption make_base_pipeline_option() const;
    PipelineOption make_transparent_pipeline_option() const;
    PipelineOption make_shadow_pipeline_option() const;
    PipelineOption make_post_pipeline_option() const;

private:
    VkWrappedInstance* ins_ = nullptr;
    Scene* scene_ = nullptr;
    Camera* camera_ = nullptr;

    std::unique_ptr<built_in_shader::BuiltInShaderMgr> shader_mgr_;
    std::vector<ForwardDrawItem> draw_items_;
    std::vector<std::string> missing_shaders_;

    built_in_shader::PhongLightUBO light_ubo_{};
    ShadowParamsUBO shadow_params_ubo_{};
    PostParamsUBO post_params_ubo_{};

    bool shadow_pipeline_ready_ = false;
    bool post_pipeline_ready_ = false;
    bool shadow_target_ready_ = false;

    VkRenderPass shadow_render_pass_ = VK_NULL_HANDLE;
    VkFramebuffer shadow_framebuffer_ = VK_NULL_HANDLE;

    uint32_t width_ = 0;
    uint32_t height_ = 0;
};

} // namespace vkkk
