#pragma once

#include <filesystem>
#include <vector>
#include <utility>

#include <vulkan/vulkan.h>
#include <spirv_cross/spirv.hpp>
#include <spirv_cross/spirv_glsl.hpp>

namespace fs = std::filesystem;

namespace vkkk
{

class ShaderModules {
public:
    ShaderModules(const VkDevice d) : device(d) {}
    virtual ~ShaderModules();

    bool add_module(fs::path path, VkShaderStageFlagBits t);
    std::vector<VkPipelineShaderStageCreateInfo> get_create_info_array() const;
    void create_descriptor_sets(const uint32_t);
    
private:
    VkDevice                                    device;
    std::vector<VkShaderModule>                 shader_modules;
    std::vector<VkShaderStageFlagBits>          shader_types;

    VkDescriptorSetLayout                       m_descriptor_layout;
    std::vector<VkDescriptorSetLayoutBinding>   m_descriptor_layout_bindings;
    VkDescriptorPool                            m_descriptor_pool;
    std::vector<VkDescriptorSet>                m_descriptor_sets;
    std::unordered_map<VkShaderStageFlagBits, spirv_cross::ShaderResources> shader_resources_map;
};

}