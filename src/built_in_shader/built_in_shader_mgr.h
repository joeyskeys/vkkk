#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan.h>

#include "vk_ins/shader_mgr.h"

namespace vkkk
{

class VkWrappedInstance;
struct PipelineOption;

namespace built_in_shader
{

enum class BuiltInShaderType : uint8_t {
    FixedColor = 0,
    Phong,
    PBR
};

struct UniformDefaultValue {
    std::string name;
    std::vector<uint8_t> bytes;
};

class BuiltInShaderMgr {
public:
    explicit BuiltInShaderMgr(VkWrappedInstance* ins);

    bool compile(BuiltInShaderType type);
    const std::vector<ShaderModule>* get_modules(BuiltInShaderType type) const;

    bool create_pipeline(const std::string& pipeline_name,
        BuiltInShaderType type, PipelineOption option);
    bool create_pipeline(const std::string& pipeline_name,
        BuiltInShaderType type, const std::vector<VERT_COMP>& comps,
        PipelineOption option);

    std::vector<UniformDefaultValue> get_default_uniforms(BuiltInShaderType type) const;
    bool apply_default_uniforms(const std::string& pipeline_name, BuiltInShaderType type) const;
    bool apply_default_uniforms(const std::string& pipeline_name, BuiltInShaderType type,
        uint32_t swapchain_image_idx) const;

private:
    struct ShaderSetSource {
        const char* vert;
        const char* frag;
    };

    std::vector<VERT_COMP> default_vertex_components(BuiltInShaderType type) const;
    ShaderSetSource get_shader_set_source(BuiltInShaderType type) const;
    bool write_uniform(const std::string& uniform_full_name,
        const std::vector<uint8_t>& bytes, uint32_t swapchain_image_idx) const;

private:
    VkWrappedInstance* ins_;
    std::unordered_map<BuiltInShaderType, std::vector<ShaderModule>> modules_;
};

} // namespace built_in_shader

} // namespace vkkk
