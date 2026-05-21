#include <cstring>
#include <iostream>
#include <utility>
#include <vector>

#include "built_in_shader/built_in_shader_mgr.h"
#include "built_in_shader/fixed_color.h"
#include "built_in_shader/pbr.h"
#include "built_in_shader/phong.h"
#include "vk_ins/vkabstraction.h"

namespace vkkk
{

namespace built_in_shader
{

namespace
{

template <typename T>
std::vector<uint8_t> to_bytes(const T& value) {
    std::vector<uint8_t> bytes(sizeof(T));
    std::memcpy(bytes.data(), &value, sizeof(T));
    return bytes;
}

} // namespace

BuiltInShaderMgr::BuiltInShaderMgr(VkWrappedInstance* ins)
    : ins_(ins)
{}

bool BuiltInShaderMgr::compile(BuiltInShaderType type) {
    auto found = modules_.find(type);
    if (found != modules_.end()) {
        return true;
    }

    const auto shader_set = get_shader_set_source(type);
    if (shader_set.vert == nullptr || shader_set.frag == nullptr) {
        return false;
    }

    std::vector<ShaderModule> modules(2);
    if (!modules[0].load(shader_set.vert, VK_SHADER_STAGE_VERTEX_BIT, "built_in_vert")) {
        return false;
    }
    if (!modules[1].load(shader_set.frag, VK_SHADER_STAGE_FRAGMENT_BIT, "built_in_frag")) {
        return false;
    }

    modules_.emplace(type, std::move(modules));
    return true;
}

const std::vector<ShaderModule>* BuiltInShaderMgr::get_modules(BuiltInShaderType type) const {
    auto found = modules_.find(type);
    if (found == modules_.end()) {
        return nullptr;
    }
    return &found->second;
}

bool BuiltInShaderMgr::create_pipeline(const std::string& pipeline_name,
    BuiltInShaderType type, PipelineOption option)
{
    return create_pipeline(pipeline_name, type, default_vertex_components(type), std::move(option));
}

bool BuiltInShaderMgr::create_pipeline(const std::string& pipeline_name,
    BuiltInShaderType type, const std::vector<VERT_COMP>& comps, PipelineOption option)
{
    if (!compile(type)) {
        return false;
    }

    auto found = modules_.find(type);
    if (found == modules_.end()) {
        return false;
    }

    return ins_->create_pipeline(pipeline_name, found->second, comps, option);
}

std::vector<UniformDefaultValue> BuiltInShaderMgr::get_default_uniforms(BuiltInShaderType type) const {
    std::vector<UniformDefaultValue> defaults;
    switch (type) {
        case BuiltInShaderType::FixedColor: {
            defaults.push_back({"UniformBufferObject", to_bytes(FixedColorTransformUBO{})});
            defaults.push_back({"FixedColor", to_bytes(FixedColorUBO{})});
            break;
        }

        case BuiltInShaderType::Phong: {
            defaults.push_back({"UniformBufferObject", to_bytes(PhongTransformUBO{})});
            defaults.push_back({"PhongMaterial", to_bytes(PhongMaterialUBO{})});
            defaults.push_back({"PhongLight", to_bytes(PhongLightUBO{})});
            break;
        }

        case BuiltInShaderType::PBR: {
            defaults.push_back({"UniformBufferObject", to_bytes(PBRTransformUBO{})});
            defaults.push_back({"PBRMaterial", to_bytes(PBRMaterialUBO{})});
            defaults.push_back({"PBRLight", to_bytes(PBRLightUBO{})});
            break;
        }
    }
    return defaults;
}

bool BuiltInShaderMgr::apply_default_uniforms(const std::string& pipeline_name,
    BuiltInShaderType type) const
{
    auto swapchain_cnt = ins_->get_swapchain_cnt();
    for (uint32_t i = 0; i < swapchain_cnt; ++i) {
        if (!apply_default_uniforms(pipeline_name, type, i)) {
            return false;
        }
    }
    return true;
}

bool BuiltInShaderMgr::apply_default_uniforms(const std::string& pipeline_name,
    BuiltInShaderType type, uint32_t swapchain_image_idx) const
{
    const auto defaults = get_default_uniforms(type);
    for (const auto& uniform : defaults) {
        if (!write_uniform(pipeline_name + ":" + uniform.name, uniform.bytes, swapchain_image_idx)) {
            return false;
        }
    }
    return true;
}

std::vector<VERT_COMP> BuiltInShaderMgr::default_vertex_components(BuiltInShaderType type) const {
    switch (type) {
        case BuiltInShaderType::FixedColor:
            return {VERTEX};
        case BuiltInShaderType::Phong:
            return {VERTEX, NORMAL};
        case BuiltInShaderType::PBR:
            return {VERTEX, NORMAL};
    }
    return {};
}

BuiltInShaderMgr::ShaderSetSource BuiltInShaderMgr::get_shader_set_source(BuiltInShaderType type) const {
    switch (type) {
        case BuiltInShaderType::FixedColor:
            return {fixed_color_vert, fixed_color_frag};
        case BuiltInShaderType::Phong:
            return {phong_vert, phong_frag};
        case BuiltInShaderType::PBR:
            return {pbr_vert, pbr_frag};
    }
    return {nullptr, nullptr};
}

bool BuiltInShaderMgr::write_uniform(const std::string& uniform_full_name,
    const std::vector<uint8_t>& bytes, uint32_t swapchain_image_idx) const
{
    auto found = ins_->ubos.find(uniform_full_name);
    if (found == ins_->ubos.end()) {
        std::cout << "Uniform " << uniform_full_name << " not found" << std::endl;
        return false;
    }

    auto& ubo = found->second;
    if (swapchain_image_idx >= ubo.memos.size()) {
        std::cout << "Swapchain image idx " << swapchain_image_idx
            << " is out of range for uniform " << uniform_full_name << std::endl;
        return false;
    }

    const uint32_t upload_size = static_cast<uint32_t>(ubo.size * ubo.vecsize);
    if (bytes.size() > upload_size) {
        std::cout << "Uniform default data too large for " << uniform_full_name << std::endl;
        return false;
    }

    std::vector<uint8_t> upload_data(upload_size, 0);
    std::memcpy(upload_data.data(), bytes.data(), bytes.size());
    ins_->sync_uniform(ubo.memos[swapchain_image_idx], upload_data.data(), upload_size);
    return true;
}

} // namespace built_in_shader

} // namespace vkkk
