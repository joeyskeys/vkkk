#include <fmt/format.h>
#include <shaderc/shaderc.hpp>

#include "utils/io.h"
#include "vk_ins/vkabstraction.h"
#include "vk_ins/shader_mgr.h"
#include "vk_ins/misc.h"

namespace vkkk
{

ShaderModulesDeprecated::ShaderModulesDeprecated(VkWrappedInstance *ins,
    UniformMgr *mgr)
    : instance(ins)
    , device(ins->get_device())
    , mem_props(ins->get_memory_props())
    , uniform_mgr(mgr)
{}

ShaderModulesDeprecated::~ShaderModulesDeprecated()
{}

void ShaderModulesDeprecated::free_gpu_resources() {
    for (const auto& shader_module : shader_modules)
        vkDestroyShaderModule(device, shader_module, nullptr);
    shader_modules.clear();
}

static GLSLTYPE find_vec_type(spirv_cross::SPIRType t) {
    enum GLSLTYPE vt = GLSLTYPE::UNKNOWN;
    assert(t.vecsize > 1);
    if (t.basetype == spirv_cross::SPIRType::Float)
        vt = static_cast<GLSLTYPE>(t.vecsize - 1);
    else if (t.basetype == spirv_cross::SPIRType::Int)
        vt = static_cast<GLSLTYPE>(2 + t.vecsize);
    return vt;
}

bool ShaderModulesDeprecated::add_module(fs::path path, VkShaderStageFlagBits t) {
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
    uint32_t binding_idx, location_idx;

    for (auto& ubo : res.uniform_buffers) {
        auto name = comp.get_name(ubo.id);
        auto type_info = comp.get_type(ubo.type_id);
        auto base_type_info = comp.get_type(ubo.base_type_id);
        binding_idx = comp.get_decoration(ubo.id, spv::DecorationBinding);
        // This code is problematic, we're assuming the array always be 1 dimension
        auto array_size = type_info.array.size() > 0 ? type_info.array[0] : 1;
        m_buf_brefs.emplace_back(name, t, comp.get_declared_struct_size(base_type_info),
            array_size, binding_idx);
    }

    for (auto& sampler : res.sampled_images) {
        binding_idx = comp.get_decoration(sampler.id, spv::DecorationBinding);
        m_img_brefs.emplace_back(sampler.name, t, binding_idx);
    }

    // Now we only handle two types of uniforms
    //uniform_info[0] = m_buf_brefs.size();
    //uniform_info[1] = m_img_brefs.size();

    // All the inputs after vertex stage comes from vertex stage in the
    // begining, skip them
    if (t == VK_SHADER_STAGE_VERTEX_BIT) {
        for (auto& input : res.stage_inputs) {
            // TODO : handle the inputs properly
            auto name = comp.get_name(input.id);
            auto type_info = comp.get_type(input.base_type_id);
            auto vectype = find_vec_type(type_info);
            location_idx = comp.get_decoration(input.id, spv::DecorationLocation);
            m_attr_brefs.emplace(location_idx, std::make_tuple(name, t, vectype));
        }
    }

    return true;
}

void ShaderModulesDeprecated::assign_tex_image(const std::string& tex_name, const std::string& tex_path, bool is_cubemap) {
    m_tex_img_pairs[tex_name] = std::make_pair(tex_path, is_cubemap);
}

void ShaderModulesDeprecated::alloc_uniforms() {
    for (auto& bref : m_buf_brefs) {
        auto [name, stage, size, vecsize, binding] = bref;
        uniform_mgr->add_buffer(name, stage, binding, size, vecsize);
    }
    for (auto& bref : m_img_brefs) {
        auto [name, stage, binding] = bref;
        const auto& path_n_qmap = m_tex_img_pairs.find(name);
        if (path_n_qmap == m_tex_img_pairs.end())
            throw std::runtime_error(fmt::format("texure image for sampler {} not assigned", name));
        auto& [path, is_cubemap] = path_n_qmap->second;
        if (!is_cubemap)
            uniform_mgr->add_texture(name, stage, binding, path);
        else
            uniform_mgr->add_cubemap(name, stage, binding, path);
    }
}

void ShaderModulesDeprecated::generate_create_infos() {
    for (int i = 0; i < shader_modules.size(); ++i) {
        VkPipelineShaderStageCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        create_info.stage = shader_types[i];
        create_info.module = shader_modules[i];
        create_info.pName = "main";
        stage_create_infos.emplace_back(std::move(create_info));
    }
}

void ShaderModulesDeprecated::set_attribute_binding(uint32_t binding_idx, uint32_t attr_location) {
    if (m_input_brefs.find(binding_idx) == m_input_brefs.end())
        m_input_brefs[binding_idx] = std::vector<uint32_t>({attr_location});
    else
        m_input_brefs[binding_idx].push_back(attr_location);
}

void ShaderModulesDeprecated::create_input_descriptions(const std::vector<VERT_COMP>& comps) {
    for (auto& [b_idx, attrs] : m_input_brefs) {
        VkVertexInputBindingDescription input_des{};
        input_des.binding = b_idx;
        uint32_t offset = 0;
        for (const auto& attr_loc : attrs) {
            const auto& attr_bref = m_attr_brefs[attr_loc];
            VkVertexInputAttributeDescription attr_des{};
            attr_des.binding = b_idx;
            attr_des.location = attr_loc;
            auto glsl_type = std::get<2>(attr_bref);
            attr_des.format = glsl_type_macro[glsl_type];
            attr_des.offset = offset;
            offset += glsl_type_sizes[glsl_type];
            m_attr_descriptions.emplace_back(std::move(attr_des));
        }

        uint32_t stride = 0;
        for (const auto& c : comps) {
            stride += comp_sizes[c] * sizeof(float);
        }
        input_des.stride = stride;

        // Fixed for now, maybe make it a parameter when we're going to
        // do instanced drawing
        input_des.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        m_input_descriptions.emplace_back(std::move(input_des));
    }
}

void ShaderModulesDeprecated::create_descriptor_layouts() {
    // Create bindings
    uint32_t binding_cnt = 0;
    auto setup_binding = [&](const auto& des, const VkDescriptorType des_type) {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = des.binding;
        binding.descriptorType = des_type;
        binding.descriptorCount = des.vecsize;
        binding.pImmutableSamplers = nullptr;
        binding.stageFlags = des.stage;
        m_descriptor_layout_bindings.emplace_back(std::move(binding));
    };

    for (auto& [ubo_name, ubo] : uniform_mgr->ubos)
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
}

void ShaderModulesDeprecated::create_descriptor_pool() {
    // Create the descriptor set layout
    auto swapchain_img_cnt = instance->get_swapchain_cnt();

    if (uniform_mgr->ubos.size() > 0)
        setup_pool(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            uniform_mgr->ubos.size() * instance->get_swapchain_cnt());
    if (uniform_mgr->textures.size() > 0)
        setup_pool(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            uniform_mgr->textures.size() * instance->get_swapchain_cnt());

    // Create the pool
    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = m_pool_sizes.size();
    pool_info.pPoolSizes = m_pool_sizes.data();
    pool_info.maxSets = swapchain_img_cnt;

    if (vkCreateDescriptorPool(device, &pool_info, nullptr, &m_descriptor_pool) != VK_SUCCESS)
        throw std::runtime_error("failed to create descriptor pool..");
}

void ShaderModulesDeprecated::create_descriptor_set() {
    auto swapchain_img_cnt = instance->get_swapchain_cnt();
    m_descriptor_sets.resize(swapchain_img_cnt);

    // Create descriptor sets
    std::vector<VkDescriptorSetLayout> layouts(swapchain_img_cnt, m_descriptor_layout);
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = m_descriptor_pool;
    alloc_info.descriptorSetCount = swapchain_img_cnt;
    alloc_info.pSetLayouts = layouts.data();
    
    if (vkAllocateDescriptorSets(device, &alloc_info, m_descriptor_sets.data()) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate descriptor sets");

    // Update it
    for (int i = 0; i < swapchain_img_cnt; i++) {
        auto writes = std::vector<VkWriteDescriptorSet>(uniform_mgr->ubos.size() +
            uniform_mgr->textures.size());
        for (int j = 0; auto& [ubo_name, ubo] : uniform_mgr->ubos) {
            ubo.update_descriptor();
            
            auto& write = writes[j++];
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = m_descriptor_sets[i];
            write.dstBinding = ubo.binding;
            write.dstArrayElement = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.descriptorCount = 1;
            write.pBufferInfo = &ubo.descriptors[i];
        }
        for (int j = 0; auto& tex : uniform_mgr->textures) {
            auto& write = writes[uniform_mgr->ubos.size() + j];
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = m_descriptor_sets[i];
            write.dstBinding = tex.binding;
            write.dstArrayElement = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.descriptorCount = 1;
            write.pImageInfo = &tex.descriptor;
            j++;
        }

        vkUpdateDescriptorSets(instance->get_device(), writes.size(), writes.data(), 0, nullptr);
    }
}

void ShaderModulesDeprecated::setup_pool(const VkDescriptorType des_type, const uint32_t cnt) {
    VkDescriptorPoolSize pool_size{};
    pool_size.type = des_type;
    pool_size.descriptorCount = cnt;
    m_pool_sizes.emplace_back(std::move(pool_size));
}

bool ShaderModule::load(const fs::path& path, const VkShaderStageFlagBits t) {
    type = t;

    auto abs_path = ensure_abs_path(path);
    auto extension = abs_path.extension();

    if (extension.string().ends_with(".spv")) {
        // Compiled SPRIV
        spirv_code = load_spirv_file(abs_path);
    }
    else {
        source_code = load_file(abs_path);

        shaderc::Compiler compiler;
        shaderc::CompileOptions options;

        // Make it a static map to lookup?
        shaderc_shader_kind tt;
        switch (t) {
            case VK_SHADER_STAGE_VERTEX_BIT: {
                tt = shaderc_glsl_vertex_shader;
                break;
            }

            case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT: {
                tt = shaderc_glsl_tess_control_shader;
                break;
            }

            case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: {
                tt = shaderc_glsl_tess_evaluation_shader;
                break;
            }

            case VK_SHADER_STAGE_GEOMETRY_BIT: {
                tt = shaderc_glsl_geometry_shader;
                break;
            }

            case VK_SHADER_STAGE_FRAGMENT_BIT: {
                tt = shaderc_glsl_fragment_shader;
                break;
            }

            case VK_SHADER_STAGE_COMPUTE_BIT: {
                tt = shaderc_glsl_fragment_shader;
                break;
            }

            default: {
                std::cout << "Shader type " << t << " not supported yet.."
                    << std::endl;
                return false;
            }
        }

        // Default to performance first
        options.SetOptimizationLevel(shaderc_optimization_level_performance);

        // entry point default to "main"
        shaderc::SpvCompilationResult ret =
            compiler.CompileGlslToSpv(source_code.data(), source_code.size(),
                tt, abs_path.filename().string().c_str(), options);
        
        if (ret.GetCompilationStatus() != shaderc_compilation_status_success) {
            std::cout << ret.GetErrorMessage();
            return false;
        }

        // Limited to the implementation of shaderc
        //auto spv_size = (ret.cend() - ret.cbegin()) * sizeof(uint32_t);
        //spirv_code.resize(spv_size);
        //memcpy(spirv_code.data(), ret.cbegin(), spv_size);
        spirv_code.insert(spirv_code.begin(), ret.cbegin(), ret.cend());
    }

    // Collecting uniform&attribute infos
    spirv_cross::CompilerGLSL comp(spirv_code);
    auto res = comp.get_shader_resources();

    // UBOs
    for (auto& ubo : res.uniform_buffers) {
        auto name = comp.get_name(ubo.id);
        auto type_info = comp.get_type(ubo.type_id);
        auto base_type_info = comp.get_type(ubo.base_type_id);
        auto binding_idx = comp.get_decoration(ubo.id, spv::DecorationBinding);
        auto struct_size = comp.get_declared_struct_size(base_type_info);
        // This code is problematic, we're assuming the array always be 1 dimension
        auto array_size = type_info.array.size() > 0 ? type_info.array[0] : 1;
        m_buf_infos.emplace(name, std::make_tuple(struct_size, array_size, binding_idx));
    }

    // Textures
    for (auto& img : res.sampled_images) {
        auto binding_idx = comp.get_decoration(img.id, spv::DecorationBinding);
        m_img_infos.emplace(img.name, binding_idx);
    }

    if (t == VK_SHADER_STAGE_VERTEX_BIT) {
        for (auto& input : res.stage_inputs) {
            auto name = comp.get_name(input.id);
            auto type_info = comp.get_type(input.base_type_id);
            auto vectype = find_vec_type(type_info);
            auto loc = comp.get_decoration(input.id, spv::DecorationLocation);
            m_attr_brefs.emplace(loc, std::make_tuple(name, vectype));
        }
    }

    return true;
}

bool ShaderModules::add_module(const fs::path& path, const VkShaderStageFlagBits t) {
    auto abs_path = ensure_abs_path(path);

    if (!fs::exists(abs_path) || !fs::is_regular_file(abs_path)) {
        std::cout << "Shader file: " << path << " does not exist or is not a file"
            << std::endl;
        return false;
    }

    auto shader_code = load_file(abs_path);
    VkShaderModuleCreateInfo module_info{};
    module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    module_info.codeSize = shader_code.size();
    module_info.pCode = reinterpret_cast<const uint32_t*>(shader_code.data());
    VkShaderModule shader_module;

    return true;
}

}