#pragma once

#include <filesystem>
#include <iostream>
#include <map>
#include <vector>
#include <utility>
#include <tuple>
#include <unordered_map>

#include <vulkan/vulkan.h>
#include <spirv_cross/spirv.hpp>
#include <spirv_cross/spirv_glsl.hpp>

#include "vk_ins/types.h"

namespace fs = std::filesystem;

namespace vkkk
{

class VkWrappedInstance;

using BufInfoWithBinding = std::tuple<std::string, VkShaderStageFlagBits,
    uint32_t, uint32_t, uint32_t>;
using BufInfoMap = std::unordered_map<std::string, std::tuple<uint32_t,
    uint32_t, uint32_t>>;
using ImgInfoWithBinding = std::tuple<std::string, VkShaderStageFlagBits,
    uint32_t>;
using ImgInfoMap = std::unordered_map<std::string, uint32_t>;
using AttrInfoWithLoc = std::tuple<std::string, VkShaderStageFlagBits,
    GLSLTYPE>;
using AttrInfoMap = std::unordered_map<uint32_t, std::tuple<std::string, uint32_t>>;
using TexImgPairs = std::unordered_map<std::string, std::pair<std::string, bool>>;

class ShaderModule {
public:
    VkShaderStageFlagBits                           type;
    std::vector<char>                               source_code;
    std::vector<uint32_t>                           spirv_code;
    //std::vector<BufInfoWithBinding>                 m_buf_brefs;
    BufInfoMap                                      buf_infos;
    //std::vector<ImgInfoWithBinding>                 m_img_brefs;
    ImgInfoMap                                      img_infos;
    std::map<uint32_t, std::vector<uint32_t>>       input_brefs;
    //std::map<uint32_t, AttrInfoWithLoc>             m_attr_brefs;
    AttrInfoMap                                     attr_infos;
    TexImgPairs                                     tex_img_pairs;

    bool load(const char* source, const VkShaderStageFlagBits t,
        const std::string& source_name="inline_shader");
    bool load(const fs::path& path, const VkShaderStageFlagBits t);
    
    std::tuple<std::string, uint32_t, uint32_t, uint32_t>
        get_uniform_info(const std::string& name) const
    {
        auto found = buf_infos.find(name);
        if (found != buf_infos.end()) {
            auto [v1, v2, v3] = found->second;
            return std::make_tuple(name, v1, v2, v3);
        }
        else
            std::cout << "No UBO for name " << name << " found.." << std::endl;
            return std::make_tuple("", 0, 0, 0);
    }
};

}