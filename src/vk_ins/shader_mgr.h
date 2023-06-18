#pragma once

#include <filesystem>
#include <vector>
#include <utility>
#include <tuple>

#include <vulkan/vulkan.h>
#include <spirv_cross/spirv.hpp>
#include <spirv_cross/spirv_glsl.hpp>

#include "uniform_mgr.h"

namespace fs = std::filesystem;

namespace vkkk
{

class VkWrappedInstance;

using texture_map = std::unordered_map<std::string, std::string>;

class ShaderModules {
public:
    ShaderModules(VkWrappedInstance *ins, UniformMgr *mgr);
    virtual ~ShaderModules();

    bool add_module(fs::path path, VkShaderStageFlagBits t);
    void assign_tex_image(const std::string& tex_name, const std::string& tex_path);
    void alloc_uniforms(const texture_map& img_paths);
    std::vector<VkPipelineShaderStageCreateInfo> get_create_info_array() const;
    void create_descriptor_pool_and_sets();

    void create_descriptor_layouts();
    void create_descriptor_pool();
    void create_descriptor_set();

    inline const VkDescriptorSet* get_descriptor_set(uint32_t idx) {
        return &m_descriptor_sets[idx];
    }

    inline const VkDescriptorSetLayout* get_descriptor_set_layout() const {
        return &m_descriptor_layout;
    }

private:
    void setup_pool(const VkDescriptorType des_type, const uint32_t cnt);
    
private:
    VkWrappedInstance*                          instance;
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

    using BufInfoWithBinding = std::tuple<std::string, VkShaderStageFlagBits,
        uint32_t, uint32_t>;
    using ImgInfoWithBinding = std::tuple<std::string, VkShaderStageFlagBits,
        uint32_t>;
    using TexImgPairs = std::unordered_map<std::string, std::string>;
    std::vector<BufInfoWithBinding> m_buf_brefs;
    std::vector<ImgInfoWithBinding> m_img_brefs;
    TexImgPairs                     m_tex_img_pairs;

    std::vector<VkDescriptorBufferInfo> m_buf_infos;
    std::vector<VkDescriptorImageInfo>  m_img_infos;
    std::vector<VkWriteDescriptorSet> m_writes;
    std::vector<VkDescriptorPoolSize> m_pool_sizes;
};

}