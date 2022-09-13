#pragma once

#include <filesystem>
#include <vector>
#include <utility>

#include <vulkan/vulkan.h>
#include <spirv_cross/spirv.hpp>
#include <spirv_cross/spirv_glsl.hpp>

#include "uniform_mgr.h"

namespace fs = std::filesystem;

namespace vkkk
{

class ShaderModules {
public:
    ShaderModules(const VkDevice d, const VkPhysicalDeviceMemoryProperties, UniformMgr *mgr);
    virtual ~ShaderModules();

    bool add_module(fs::path path, VkShaderStageFlagBits t);
    void alloc_uniforms(const uint32_t swapchain_img_cnt, const std::unordered_map<std::string, std::string>& img_paths);
    std::vector<VkPipelineShaderStageCreateInfo> get_create_info_array() const;
    void create_descriptor_sets(const uint32_t);
    
private:
    VkDevice                                    device;
    VkPhysicalDeviceMemoryProperties            mem_props;
    UniformMgr*                                 uniform_mgr;
    std::vector<VkShaderModule>                 shader_modules;
    std::vector<VkShaderStageFlagBits>          shader_types;

    VkDescriptorSetLayout                       m_descriptor_layout;
    std::vector<VkDescriptorSetLayoutBinding>   m_descriptor_layout_bindings;
    VkDescriptorPool                            m_descriptor_pool;
    std::vector<VkDescriptorSet>                m_descriptor_sets;
    std::unordered_map<VkShaderStageFlagBits, spirv_cross::ShaderResources> shader_resources_map;

    using BufferResources = std::tuple<std::vector<VkBuffer>, std::vector<VkDeviceMemory>>;
    using ImageResources = std::tuple<VkImage, VkDeviceMemory, VkImageView, VkSampler>;
    std::unordered_map<VkShaderStageFlagBits, std::vector<BufferResources>> m_ubo_resources;
    std::unordered_map<VkShaderStageFlagBits, std::vector<ImageResources>> m_img_resources;
    std::unordered_map<VkShaderStageFlagBits, std::vector<VkSampler>> m_sampler_resources;

    using BufInfoWithBinding = std::pair<VkDescriptorBufferInfo, uint32_t>;
    using ImgInfoWithBinding = std::pair<VkDescriptorImageInfo, uint32_t>;
    std::unordered_map<VkShaderStageFlagBits, std::vector<BufInfoWithBinding>> m_ubo_infos;
    std::unordered_map<VkShaderStageFlagBits, std::vector<ImgInfoWithBinding>> m_img_infos;

    std::vector<VkWriteDescriptorSet> m_writes;
    std::vector<VkDescriptorPoolSize> m_pool_sizes;
};

}