#pragma once

#include <unordered_map>

#include <vulkan/vulkan_raii.hpp>

namespace vkkk
{

using BufInfoMap = std::unordered_map<std::string, std::tuple<uint32_t,
    uint32_t, uint32_t>>;
using ImgInfoMap = std::unordered_map<std::string, uint32_t>;
using AttrInfoMap = std::unordered_map<std::string, std::tuple<uint32_t, uint32_t>>;
using TexImgPairs = std::unordered_map<std::string, std::pair<std::string, bool>>;

class ShaderModule {
public:
    VkShaderStageFlagBits                           type;
    std::vector<char>                               source_code;
    std::vector<uint32_t>                           spirv_code;
    BufInfoMap                                      buf_infos;
    ImgInfoMap                                      img_infos;
    AttrInfoMap                                     attr_infos;
    TexImgPairs                                     tex_img_pairs;

    bool load(const char* source, const vk::ShaderStageFlagBits t,
        const std::string& source_name="inline_shader");
    bool load(const fs::path& path, const vk::ShaderStageFlagBits t);
    
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

class ShaderModulePack {
public:
    ShaderModulePack();

    bool add_shader_module(const ShaderModule& module, bool replace=false);

public:
    std::unordered_map<vk::ShaderStageFlagBits, ShaderModule> modules;
}

}