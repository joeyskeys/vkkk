#include "utils/io.h"
#include "vk_ins/shader_mgr.h"

namespace vkkk
{

ShaderModules::~ShaderModules() {
    for (const auto &shader_module : shader_modules)
        vkDestroyShaderModule(device, shader_module, nullptr);
}

bool ShaderModules::add_module(fs::path path, VkShaderStageFlagBits t) {
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

    // Collecting descriptor infos
    auto spirv_code = load_spirv_file(path);
    spirv_cross::CompilerGLSL comp(std::move(spirv_code));
    auto res = comp.get_shader_resources();
    shader_resources_map[t] = res;

    auto binding_setup = [&](const auto& res, const VkDescriptorType des_type) {
        auto type_info = comp.get_type(res.id);

        VkDescriptorSetLayoutBinding res_binding{};
        // Notice : this will bring a dependency of the module adding order with the binding
        // number. For example if we add fragment shader first, the uniforms in that shader
        // will get the binding number starting from zero
        res_binding.binding = m_descriptor_layout_bindings.size();
        res_binding.descriptorType = des_type;
        // To have the vecsize info is why we do layout binding setup here
        res_binding.descriptorCount = type_info.vecsize;
        res_binding.pImmutableSamplers = nullptr;
        res_binding.stageFlags = t;
        m_descriptor_layout_bindings.emplace_back(std::move(res_binding));
    };

    for (const auto& ubo : res.uniform_buffers)
        binding_setup(ubo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    for (const auto& sampler : res.separate_samplers)
        binding_setup(sampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    return true;
}

std::vector<VkPipelineShaderStageCreateInfo> ShaderModules::get_create_info_array() const {
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

void ShaderModules::create_descriptor_sets(const uint32_t swapchain_img_cnt) {
    // Create descriptor pools
    std::vector<VkDescriptorPoolSize> pool_sizes{};

    for (const auto& shader_resources_pair : shader_resources_map) {
        for (const auto& ubo : shader_resources_pair.second.uniform_buffers) {
            VkDescriptorPoolSize pool_size{};
            pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            pool_size.descriptorCount = swapchain_img_cnt;
            pool_sizes.emplace_back(std::move(pool_size));
        }

        for (const auto& sampler : shader_resources_pair.second.separate_samplers) {
            VkDescriptorPoolSize pool_size{};
            pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            pool_size.descriptorCount = swapchain_img_cnt;
            pool_sizes.emplace_back(std::move(pool_size));
        }
    }

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = pool_sizes.size();
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = swapchain_img_cnt;

    if (vkCreateDescriptorPool(device, &pool_info, nullptr, &m_descriptor_pool) != VK_SUCCESS)
        throw std::runtime_error("failed to create descriptor pool..");

    // Create descriptor set layout
    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = m_descriptor_layout_bindings.size();
    layout_info.pBindings = m_descriptor_layout_bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &m_descriptor_layout) != VK_SUCCESS)
        throw std::runtime_error("failed to create descriptor set layout");

    // Create descriptor sets
    std::vector<VkDescriptorSetLayout> layouts(swapchain_img_cnt, m_descriptor_layout);
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = m_descriptor_pool;
    alloc_info.descriptorSetCount = swapchain_img_cnt;
    alloc_info.pSetLayouts = layouts.data();

    m_descriptor_sets.resize(swapchain_img_cnt);
    if (vkAllocateDescriptorSets(device, &alloc_info, m_descriptor_sets.data()) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate descriptor sets");

    for (int i = 0; i < swapchain_img_cnt; i++) {
        VkDescriptorBufferInfo buf_info{};
        // Need a UniformMgr here, for both CPU side and GPU side
        //buf_info.buffer = 
    }
}

}