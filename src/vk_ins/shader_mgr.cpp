#include "utils/io.h"
#include "vk_ins/shader_mgr.h"

namespace vkkk
{

ShaderModules::~ShaderModules() {
    for (const auto &shader_module : shader_modules)
        vkDestroyShaderModule(device, shader_module, nullptr);
}

bool ShaderModules::add_module(fs::path path, VkDevice &device, VkShaderStageFlagBits t) {
    fs::path abs_path = path;
    if (path.is_relative())
        abs_path = fs::absolute(path);
    
    if (!fs::exists(abs_path) || !fs::is_regular_file(abs_path)) {
        std::cerr << "Shader file : " << path << " does not exist or is not a file" << std::endl;
        return false;
    }

    auto shader_code = load_file(path);
    VkShaderModuleCreateInfo module_create_info{};
    module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    module_create_info.codeSize = shader_code.size();
    module_create_info.pCode = reinterpret_cast<const uint32_t*>(shader_code.data());
    VkShaderModule shader_module;
    if (vkCreateShaderModule(device, &module_create_info, nullptr, &shader_module) != VK_SUCCESS)
        std::cerr << "Shader " << path.filename() << " create failed" << std::endl;

    shader_modules.push_back(shader_module);
    shader_types.push_back(t);
}

auto ShaderModules::get_create_info_array() const {
    std::vector<VkPipelineShaderStageCreateInfo> stage_create_infos;
    for (int i = 0; i < shader_modules.size(); ++i) {
        VkPipelineShaderStageCreateInfo stage_create_info{};
        stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_create_info.stage = shader_types[i];
        stage_create_info.module = shader_modules[i];
        // For now we follow the common convention of main as the entry
        stage_create_info.pName = "main";

        stage_create_infos.emplace_back(std::move(stage_create_info));
    }

    return stage_create_infos;
}

}