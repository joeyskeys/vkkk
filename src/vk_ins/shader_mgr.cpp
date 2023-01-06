#include "utils/io.h"
#include "vk_ins/vkabstraction.h"
#include "vk_ins/shader_mgr.h"
#include "vk_ins/misc.h"

namespace vkkk
{

ShaderModules::ShaderModules(VkWrappedInstance *ins,
    UniformMgr *mgr)
    : instance(ins)
    , device(ins->get_device())
    , mem_props(ins->get_memory_props())
    , uniform_mgr(mgr)
{}

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
    spirv_cross::SPIRType type_info;
    uint32_t binding_idx;

    /*
    auto setup = [&](const auto& res, const VkDescriptorType des_type) {
        // Descriptor set layout
        VkDescriptorSetLayoutBinding res_binding{};
        res_binding.binding = binding_idx;
        res_binding.descriptorType = des_type;
        // To have the vecsize info is why we do layout binding setup here
        res_binding.descriptorCount = type_info.vecsize;
        res_binding.pImmutableSamplers = nullptr;
        res_binding.stageFlags = t;
        // Will different ordering of layout binding array and write descriptor
        // sets cause a performance issue or even error?
        m_descriptor_layout_bindings.emplace_back(std::move(res_binding));
    };

    std::vector<BufInfoWithBinding> buf_infos{};
    for (auto& ubo : res.uniform_buffers) {
        type_info = comp.get_type(ubo.base_type_id);
        binding_idx = m_descriptor_layout_bindings.size();
        setup(ubo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

        // Construct ubo info
        VkDescriptorBufferInfo buf_info{};
        buf_info.offset = 0;
        buf_info.range = comp.get_declared_struct_size(type_info);
        buf_infos.emplace_back(std::make_pair(std::move(buf_info), binding_idx));
    }
    m_ubo_infos.emplace(t, std::move(buf_infos));

    std::vector<ImgInfoWithBinding> img_infos{};
    for (auto& sampler : res.separate_samplers) {
        type_info = comp.get_type(sampler.id);
        binding_idx = m_descriptor_layout_bindings.size();
        setup(sampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

        // Construct sampler info
        VkDescriptorImageInfo img_info{};
        img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        // The actual texture view and sampler creation is postponed
        // Make the DescriptorImageInfo here as a placeholder
        img_infos.emplace_back(std::make_pair(std::move(img_info), binding_idx));
    }
    m_img_infos.emplace(t, std::move(img_infos));
    */

    for (const auto& shader_resources_pair : shader_resources_map) {
        for (auto& ubo : res.uniform_buffers) {
            type_info = comp.get_type(ubo.base_type_id);
            m_buf_brefs.emplace_back(ubo.name, t, comp.get_declared_struct_size(type_info));
        }

        for (auto& sampler : res.sampled_images) {
            m_img_brefs.emplace_back(sampler.name, t);
        }
    }

    return true;
}

void ShaderModules::alloc_uniforms(const texture_map& img_paths) {
    /*
    for (const auto& shader_resources_pair : shader_resources_map) {
        for (int i = 0; i < shader_resources_pair.second.uniform_buffers.size(); i++) {
            const auto& ubo = shader_resources_pair.second.uniform_buffers[i];
            uniform_mgr->add_buffer(ubo.name, shader_resources_pair.first,
                buf_info.first.range);
        }

        if (img_paths.size() == 0)
            continue;

        for (int i = 0; i < shader_resources_pair.second.separate_samplers.size(); i++) {
            auto& sampler = shader_resources_pair.second.separate_samplers[i];
            uniform_mgr->add_texture(img_paths.at(sampler.name),
                shader_resources_pair.first);
        }
    }
    */

    for (auto& bref : m_buf_brefs) {
        auto [name, stage, size] = bref;
        uniform_mgr->add_buffer(name, stage, size);
    }
    for (auto& bref : m_img_brefs) {
        auto [name, stage] = bref;
        uniform_mgr->add_texture(name, stage);
    }
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

void ShaderModules::create_descriptor_pool_and_sets() {
    // Create the descriptor set layout
    VkDescriptorSetLayoutBinding layout_binding{};
    std::vector<VkDescriptorPoolSize> pool_sizes{};
    auto swapchain_img_cnt = instance->get_swapchain_cnt();
    m_descriptor_sets.resize(swapchain_img_cnt);

    setup_pool(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        uniform_mgr->ubos.size() * instance->get_swapchain_cnt());
    setup_pool(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        uniform_mgr->textures.size() * instance->get_swapchain_cnt());

    // Create the pool
    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = pool_sizes.size();
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = swapchain_img_cnt;

    if (vkCreateDescriptorPool(device, &pool_info, nullptr, &m_descriptor_pool) != VK_SUCCESS)
        throw std::runtime_error("failed to create descriptor pool..");

    // Create bindings
    uint32_t binding_cnt = 0;
    auto setup_binding = [&](const auto& des, const VkDescriptorType des_type) {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = binding_cnt++;
        binding.descriptorType = des_type;
        binding.descriptorCount = des.vecsize;
        binding.pImmutableSamplers = nullptr;
        binding.stageFlags = des.stage;
        m_descriptor_layout_bindings.emplace_back(std::move(binding));
    };

    for (auto& ubo : uniform_mgr->ubos)
        setup_binding(ubo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    for (auto& tex : uniform_mgr->textures)
        setup_binding(tex, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    
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
    
    if (vkAllocateDescriptorSets(device, &alloc_info, m_descriptor_sets.data()) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate descriptor sets");

    // Create descriptor info & write
    /*
    m_buf_infos.reserve(uniform_mgr->ubos.size());
    m_img_infos.reserve(uniform_mgr->textures.size());
    m_writes.reserve(uniform_mgr->ubos.size() + uniform_mgr->textures.size());
    binding_cnt = 0;
    for (auto& ubo : uniform_mgr->ubos) {
        VkDescriptorBufferInfo buf_info{};
        buf_info.offset = 0;
        buf_info.range = ubo.size * ubo.vecsize;
        m_buf_infos.emplace_back(std::move(buf_info));

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstBinding = binding_cnt++;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &m_buf_infos.back();
        m_writes.emplace_back(std::move(write));
    }
    for (auto& tex : uniform_mgr->textures) {
        VkDescriptorImageInfo img_info{};
        img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        img_info.imageView = tex.view;
        img_info.sampler = tex.sampler;
        m_img_infos.emplace_back(std::move(img_info));

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstBinding = binding_cnt++;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &m_img_infos.back();
        m_writes.emplace_back(std::move(write));
    }

    for (size_t i = 0; i < swapchain_img_cnt; i++) {
        for (int j = 0; auto& ubo : uniform_mgr->ubos) {
            m_buf_infos[j].buffer = ubo.gpu_bufs[i];
            m_writes[j].dstSet = m_descriptor_sets[i];
            ++j;
        }

        auto ubo_cnt = uniform_mgr->ubos.size();
        for (int j = 0; auto& tex : uniform_mgr->textures) {
            m_writes[ubo_cnt + j].dstSet = m_descriptor_sets[i];
            ++j;
        }

        vkUpdateDescriptorSets(device, m_writes.size(), m_writes.data(), 0, nullptr);
    }
    */

    for (int i = 0; i < swapchain_img_cnt; i++) {
        auto writes = std::vector<VkWriteDescriptorSet>(uniform_mgr->ubos.size() +
            uniform_mgr->textures.size());
        auto buf_infos = std::vector<VkDescriptorBufferInfo>(uniform_mgr->ubos.size());
        auto tex_infos = std::vector<VkDescriptorImageInfo>(uniform_mgr->textures.size());
        binding_cnt = 0;
        for (int j = 0; auto& ubo : uniform_mgr->ubos) {
            auto& buf_info = buf_infos[j];
            buf_info.offset = 0;
            buf_info.range = ubo.size * ubo.vecsize;
            buf_info.buffer = ubo.gpu_bufs[i];
            
            auto& write = writes[j++];
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = m_descriptor_sets[i];
            write.dstBinding = binding_cnt++;
            write.dstArrayElement = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.descriptorCount = 1;
            write.pBufferInfo = &buf_info;
        }
        for (int j = 0; auto& tex : uniform_mgr->textures) {
            auto tex_info = tex_infos[j];
            tex_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            tex_info.imageView = tex.view;
            tex_info.sampler = tex.sampler;

            auto& write = writes[uniform_mgr->ubos.size() + j];
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = m_descriptor_sets[i];
            write.dstBinding = binding_cnt++;
            write.dstArrayElement = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.descriptorCount = 1;
            write.pImageInfo = &tex_info;
            j++;
        }

        vkUpdateDescriptorSets(instance->get_device(), writes.size(), writes.data(), 0, nullptr);
    }
}

void ShaderModules::setup_pool(const VkDescriptorType des_type, const uint32_t cnt) {
    VkDescriptorPoolSize pool_size{};
    pool_size.type = des_type;
    pool_size.descriptorCount = cnt;
    m_pool_sizes.emplace_back(std::move(pool_size));
}

}