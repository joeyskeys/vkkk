#pragma once

#include <filesystem>
#include <iostream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

#include "vk_ins/types.h"

namespace fs = std::filesystem;

namespace vkkk
{

using BufInfoMap = std::unordered_map<std::string, std::tuple<uint32_t, uint32_t, uint32_t>>;
using StorageBufInfoMap = std::unordered_map<std::string, std::tuple<uint32_t, uint32_t, uint32_t>>;
using ImgInfoMap = std::unordered_map<std::string, uint32_t>;
using AttrInfoMap = std::vector<std::tuple<uint32_t, uint32_t, std::string>>;
using TexImgPairs = std::unordered_map<std::string, std::pair<std::string, bool>>;
// Keyed by GLSL block/type name → (byte size, byte offset in the push-constant space).
using PushConstantInfoMap = std::unordered_map<std::string, std::tuple<uint32_t, uint32_t>>;

class ShaderModule {
public:
    vk::ShaderStageFlagBits                         type{};
    std::vector<char>                               source_code;
    std::vector<uint32_t>                           spirv_code;
    BufInfoMap                                      buf_infos;
    StorageBufInfoMap                               storage_buf_infos;
    ImgInfoMap                                      img_infos;
    AttrInfoMap                                     attr_infos;
    TexImgPairs                                     tex_img_pairs;
    PushConstantInfoMap                             push_constant_infos;

    bool load(const char* source, vk::ShaderStageFlagBits t,
        const std::string& source_name = "inline_shader");
    bool load(const fs::path& path, vk::ShaderStageFlagBits t);

    std::tuple<std::string, uint32_t, uint32_t, uint32_t>
    get_uniform_info(const std::string& name) const
    {
        auto found = buf_infos.find(name);
        if (found != buf_infos.end()) {
            auto [v1, v2, v3] = found->second;
            return std::make_tuple(name, v1, v2, v3);
        }
        std::cout << "No UBO for name " << name << " found.." << std::endl;
        return std::make_tuple("", 0, 0, 0);
    }
};

class ShaderModulePack {
public:
    ShaderModulePack() = default;

    bool add_shader_module(const ShaderModule& module, bool replace = false);
    bool uses_mesh_shader() const { return use_mesh_shader; }

    std::unordered_map<vk::ShaderStageFlagBits, ShaderModule> modules;
    bool use_mesh_shader = false;
};

} // namespace vkkk
