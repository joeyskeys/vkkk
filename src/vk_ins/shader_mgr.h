#pragma once

#include <filesystem>
#include <map>
#include <vector>
#include <utility>
#include <tuple>

#include <vulkan/vulkan.h>
#include <spirv_cross/spirv.hpp>
#include <spirv_cross/spirv_glsl.hpp>

#include "vk_ins/types.h"
#include "vk_ins/uniform_mgr.h"

namespace fs = std::filesystem;

namespace vkkk
{

class VkWrappedInstance;

using BufInfoWithBinding = std::tuple<std::string, VkShaderStageFlagBits,
    uint32_t, uint32_t, uint32_t>;
using ImgInfoWithBinding = std::tuple<std::string, VkShaderStageFlagBits,
    uint32_t>;
using AttrInfoWithLoc = std::tuple<std::string, VkShaderStageFlagBits,
    GLSLTYPE>;
using TexImgPairs = std::unordered_map<std::string, std::pair<std::string, bool>>;

class ShaderModulesDeprecated {
public:
    ShaderModulesDeprecated(VkWrappedInstance *ins, UniformMgr *mgr);
    virtual ~ShaderModulesDeprecated();

    void free_gpu_resources();

    bool add_module(fs::path path, VkShaderStageFlagBits t);
    void assign_tex_image(const std::string& tex_name, const std::string& tex_path, bool is_cubemap=false);
    void alloc_uniforms();
    void generate_create_infos();
    //std::vector<VkPipelineShaderStageCreateInfo> get_create_info_array() const;

    void set_attribute_binding(uint32_t binding_idx, uint32_t attr_location);
    void create_input_descriptions(const std::vector<VERT_COMP>&);
    void create_descriptor_layouts();
    void create_descriptor_pool();
    void create_descriptor_set();

    inline bool valid() {
        return shader_types.size() > 1 &&
            std::find(shader_types.begin(), shader_types.end(), VK_SHADER_STAGE_VERTEX_BIT) != shader_types.end() &&
            std::find(shader_types.begin(), shader_types.end(), VK_SHADER_STAGE_FRAGMENT_BIT) != shader_types.end();
    }

    inline uint32_t get_stages_count() const {
        return stage_create_infos.size();
    }

    inline const VkPipelineShaderStageCreateInfo* get_stages_data() const {
        return stage_create_infos.data();
    }

    inline uint32_t get_binding_description_count() const {
        return m_input_descriptions.size();
    }

    inline const VkVertexInputBindingDescription* get_binding_descriptions() const {
        return m_input_descriptions.data();
    }

    inline uint32_t get_attr_description_count() const {
        return m_attr_descriptions.size();
    }

    inline const VkVertexInputAttributeDescription* get_attr_descriptions() const {
        return m_attr_descriptions.data();
    }

    inline const VkDescriptorSet* get_descriptor_set(uint32_t idx) {
        return &m_descriptor_sets[idx];
    }

    inline const VkDescriptorSetLayout* get_descriptor_set_layout() const {
        return &m_descriptor_layout;
    }

private:
    void setup_pool(const VkDescriptorType des_type, const uint32_t cnt);
    
private:
    VkWrappedInstance*                              instance;
    VkDevice                                        device;
    VkPhysicalDeviceMemoryProperties                mem_props;
    UniformMgr*                                     uniform_mgr;
    std::vector<VkShaderModule>                     shader_modules;
    std::vector<VkShaderStageFlagBits>              shader_types;
    std::vector<VkPipelineShaderStageCreateInfo>    stage_create_infos;

    std::vector<VkVertexInputBindingDescription>    m_input_descriptions;
    std::vector<VkVertexInputAttributeDescription>  m_attr_descriptions;

    VkDescriptorSetLayout                           m_descriptor_layout;
    std::vector<VkDescriptorSetLayoutBinding>       m_descriptor_layout_bindings;
    VkDescriptorPool                                m_descriptor_pool;
    std::vector<VkDescriptorSet>                    m_descriptor_sets;
    std::unordered_map<VkShaderStageFlagBits, spirv_cross::ShaderResources> shader_resources_map;

    using BufferResources = std::tuple<std::vector<VkBuffer>, std::vector<VkDeviceMemory>>;
    using ImageResources = std::tuple<VkImage, VkDeviceMemory, VkImageView, VkSampler>;
    std::unordered_map<VkShaderStageFlagBits, std::vector<BufferResources>> m_ubo_resources;
    std::unordered_map<VkShaderStageFlagBits, std::vector<ImageResources>> m_img_resources;
    std::unordered_map<VkShaderStageFlagBits, std::vector<VkSampler>> m_sampler_resources;

    std::vector<BufInfoWithBinding>                 m_buf_brefs;
    std::vector<ImgInfoWithBinding>                 m_img_brefs;
    std::map<uint32_t, std::vector<uint32_t>>       m_input_brefs;
    std::map<uint32_t, AttrInfoWithLoc>             m_attr_brefs;
    TexImgPairs                                     m_tex_img_pairs;

    std::vector<VkWriteDescriptorSet>               m_writes;
    std::vector<VkDescriptorPoolSize>               m_pool_sizes;
};

class ShaderModule {
public:
    VkShaderStageFlagBits                           type;
    std::vector<char>                               source_code;
    std::vector<char>                               spirv_code;
    std::vector<BufInfoWithBinding>                 m_buf_brefs;
    std::vector<ImgInfoWithBinding>                 m_img_brefs;
    std::map<uint32_t, std::vector<uint32_t>>       m_input_brefs;
    std::map<uint32_t, AttrInfoWithLoc>             m_attr_brefs;
    TexImgPairs                                     m_tex_img_pairs;
};

class ShaderModules {
public:
    bool add_module(fs::path path, VkShaderStageFlagBits t);

private:
    VkWrappedInstance*                              ins;
    std::vector<VkShaderModule>                     modules;
    std::vector<VkShaderStageFlagBits>              types;

    std::vector<BufInfoWithBinding>                 m_buf_brefs;
    std::vector<ImgInfoWithBinding>                 m_img_brefs;
    std::map<uint32_t, std::vector<uint32_t>>       m_input_brefs;
    std::map<uint32_t, AttrInfoWithLoc>             m_attr_brefs;
    TexImgPairs                                     m_tex_img_pairs;
};

}