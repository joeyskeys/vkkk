#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>

#include "vk_ins/context.hpp"

namespace vkkk
{

static std::vector<vk::VertexInputBindingDescription> gen_binding_desc(const std::vector<VERT_COMP>& comps, bool interleaved) {
    // a vertex binding itself being interleaved or not is irrelavent to others
    // the mesh format can be quite flexible, for example, vertices being a single buf,
    // uv and color interleaved being another buf. Each binding desc is related to its
    // attr desc.
    // Here we assume the mesh being either all interleaved or all separated.
    std::vector<vk::VertexInputBindingDescription> binding_descriptions;
    if (interleaved) {
        binding_descriptions.emplace_back(0, get_mesh_component_size(comps) * sizeof(float), vk::VertexInputRate::eVertex);
    }
    else {
        for (const auto& comp : comps) {
            binding_descriptions.emplace_back(0, comp_sizes[comp] * sizeof(float), vk::VertexInputRate::eVertex);
        }
    }
    return binding_descriptions;
}

bool Context::create_pipeline(const std::string& name,
    const ShaderModulePack& shader_module_pack,
    const PipelineOption& option,
    const std::vector<VERT_COMP>& comps,
    bool interleaved,
    bool depth_only,
    const std::vector<vk::Format>& color_formats)
{
    if (pipelines.find(name) != pipelines.end()) {
        std::cout << "Pipeline " << name << " already exists" << std::endl;
        return false;
    }

    std::vector<vk::VertexInputBindingDescription> input_binding_descs;
    std::vector<vk::VertexInputAttributeDescription> input_attr_descs;
    std::map<uint32_t, vk::DescriptorSetLayoutBinding> descriptor_bindings;
    std::map<uint32_t, std::string> ubo_binding_to_name;
    std::map<uint32_t, std::string> storage_binding_to_name;
    std::map<uint32_t, std::string> tex_binding_to_name;
    std::unordered_map<uint32_t, uint32_t> sampler_descriptor_counts;
    std::unordered_map<std::string, UBO> pipeline_ubos;
    std::unordered_map<std::string, SSBO> pipeline_ssbos;
    const bool pipeline_uses_mesh_shader = shader_module_pack.uses_mesh_shader()
        || shader_module_pack.modules.contains(vk::ShaderStageFlagBits::eMeshEXT)
        || shader_module_pack.modules.contains(vk::ShaderStageFlagBits::eTaskEXT);
    bool has_vertex_stage = false;
    bool has_mesh_stage = false;
    bool has_task_stage = false;

    if (pipeline_uses_mesh_shader && (!use_mesh_shader || !mesh_shader_available)) {
        std::cout << "Pipeline " << name << " requested mesh shader, but mesh shader support is disabled." << std::endl;
        return false;
    }

    std::vector<vk::raii::ShaderModule> shader_modules;
    std::vector<vk::PipelineShaderStageCreateInfo> shader_stage_infos;
    shader_modules.reserve(shader_module_pack.modules.size());
    shader_stage_infos.reserve(shader_module_pack.modules.size());

    const auto merge_descriptor_binding = [&descriptor_bindings](const vk::DescriptorSetLayoutBinding& binding) {
        const auto found = descriptor_bindings.find(binding.binding);
        if (found == descriptor_bindings.end()) {
            descriptor_bindings.emplace(binding.binding, binding);
            return;
        }
        found->second.stageFlags |= binding.stageFlags;
    };

    for (const auto& [stage, module] : shader_module_pack.modules) {
        vk::ShaderModuleCreateInfo shader_module_create_info{};
        shader_module_create_info.codeSize = module.spirv_code.size() * sizeof(uint32_t);
        shader_module_create_info.pCode = module.spirv_code.data();
        shader_modules.emplace_back(device, shader_module_create_info);

        vk::PipelineShaderStageCreateInfo shader_stage_info{};
        shader_stage_info.stage = stage;
        shader_stage_info.module = *shader_modules.back();
        shader_stage_info.pName = "main";
        shader_stage_infos.push_back(shader_stage_info);

        if (stage == vk::ShaderStageFlagBits::eTaskEXT) {
            has_task_stage = true;
        }
        if (stage == vk::ShaderStageFlagBits::eMeshEXT) {
            has_mesh_stage = true;
        }
        if (stage == vk::ShaderStageFlagBits::eVertex) {
            has_vertex_stage = true;
            input_binding_descs = gen_binding_desc(comps, interleaved);

            uint32_t offset = 0;
            int binding_idx = 0;
            for (const auto& [attr_loc, glsl_type, attr_name] : module.attr_infos) {
                (void)attr_name;
                if (interleaved) {
                    vk::VertexInputAttributeDescription attr_desc{};
                    attr_desc.location = attr_loc;
                    attr_desc.binding = 0;
                    attr_desc.format = static_cast<vk::Format>(glsl_type_macro[glsl_type]);
                    attr_desc.offset = offset;
                    input_attr_descs.push_back(attr_desc);
                    offset += glsl_type_sizes[glsl_type];
                }
                else {
                    vk::VertexInputAttributeDescription attr_desc{};
                    attr_desc.location = attr_loc;
                    attr_desc.binding = static_cast<uint32_t>(binding_idx);
                    attr_desc.format = static_cast<vk::Format>(glsl_type_macro[glsl_type]);
                    attr_desc.offset = 0;
                    input_attr_descs.push_back(attr_desc);
                }
                ++binding_idx;
            }
        }

        for (const auto& [ubo_name, ubo_info] : module.buf_infos) {
            const auto& [struct_size, array_size, binding] = ubo_info;
            const uint32_t alloc_size = struct_size == 0 ? 16u : struct_size;
            add_ubo(pipeline_ubos, ubo_name, binding, alloc_size, array_size);
            ubo_binding_to_name[binding] = ubo_name;

            vk::DescriptorSetLayoutBinding layout_binding{};
            layout_binding.binding = binding;
            layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
            layout_binding.descriptorCount = array_size;
            layout_binding.stageFlags = stage;
            merge_descriptor_binding(layout_binding);
        }

        if (!module.storage_buf_infos.empty()) {
            for (const auto& [ssbo_name, ssbo_info] : module.storage_buf_infos) {
                const auto& [struct_size, array_size, binding] = ssbo_info;
                const auto existing = pipeline_ssbos.find(ssbo_name);
                if (existing == pipeline_ssbos.end()) {
                    SSBO ssbo{};
                    ssbo.size = struct_size == 0 ? 16u : struct_size;
                    ssbo.vecsize = array_size;
                    ssbo.binding = binding;
                    ssbo.descriptor_type = vk::DescriptorType::eStorageBuffer;
                    pipeline_ssbos.emplace(ssbo_name, std::move(ssbo));
                }
                else if (existing->second.binding != binding) {
                    std::cout << "SSBO " << ssbo_name << " uses inconsistent bindings in pipeline "
                        << name << std::endl;
                    return false;
                }

                storage_binding_to_name[binding] = ssbo_name;
                vk::DescriptorSetLayoutBinding layout_binding{};
                layout_binding.binding = binding;
                layout_binding.descriptorType = vk::DescriptorType::eStorageBuffer;
                layout_binding.descriptorCount = 1;
                layout_binding.stageFlags = stage;
                merge_descriptor_binding(layout_binding);
            }
        }

        for (const auto& [tex_name, tex_binding] : module.img_infos) {
            const auto ppl_tex_name = name + ":" + tex_name;
            uint32_t descriptor_count = 1;
            sampler_descriptor_counts[tex_binding] = descriptor_count;
            if (textures.find(ppl_tex_name) == textures.end()) {
                const auto tex_path_info = module.tex_img_pairs.find(tex_name);
                if (tex_path_info == module.tex_img_pairs.end()) {
                    std::cout << "No texture assigned for sampler " << tex_name << std::endl;
                }
                else {
                    const auto& [path, is_cubemap] = tex_path_info->second;
                    descriptor_count = is_cubemap ? 6u : 1u;
                    if (!is_cubemap) {
                        if (add_texture(ppl_tex_name, tex_binding, path)) {
                            tex_binding_to_name[tex_binding] = ppl_tex_name;
                        }
                    }
                    else if (add_cubemap(ppl_tex_name, tex_binding, path)) {
                        tex_binding_to_name[tex_binding] = ppl_tex_name;
                    }
                }
            }
            else {
                descriptor_count = static_cast<uint32_t>(textures.at(ppl_tex_name).vecsize);
                tex_binding_to_name[tex_binding] = ppl_tex_name;
            }
            sampler_descriptor_counts[tex_binding] = descriptor_count;

            vk::DescriptorSetLayoutBinding tex_layout_binding{};
            tex_layout_binding.binding = tex_binding;
            tex_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
            tex_layout_binding.descriptorCount = descriptor_count;
            tex_layout_binding.stageFlags = stage;
            merge_descriptor_binding(tex_layout_binding);
        }
    }

    std::vector<vk::DescriptorSetLayoutBinding> descriptor_layouts;
    descriptor_layouts.reserve(descriptor_bindings.size());
    for (const auto& [_, binding] : descriptor_bindings) {
        descriptor_layouts.push_back(binding);
    }

    if (!pipeline_uses_mesh_shader && !has_vertex_stage) {
        std::cout << "Pipeline " << name << " has no vertex stage." << std::endl;
        return false;
    }
    if (pipeline_uses_mesh_shader && has_vertex_stage) {
        std::cout << "Pipeline " << name << " mixes mesh/task and vertex shader stages." << std::endl;
        return false;
    }
    if (has_task_stage && !task_shader_available) {
        std::cout << "Pipeline " << name << " uses task shader, but task shader support is disabled." << std::endl;
        return false;
    }
    if (has_task_stage && !has_mesh_stage) {
        std::cout << "Pipeline " << name << " has a task stage without a mesh stage." << std::endl;
        return false;
    }
    if (pipeline_uses_mesh_shader && !has_mesh_stage) {
        std::cout << "Pipeline " << name << " requested mesh shader path without a mesh stage." << std::endl;
        return false;
    }

    PipelineOption local_option = option;
    local_option.vert_info.vertexBindingDescriptionCount = static_cast<uint32_t>(input_binding_descs.size());
    local_option.vert_info.pVertexBindingDescriptions = input_binding_descs.data();
    local_option.vert_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(input_attr_descs.size());
    local_option.vert_info.pVertexAttributeDescriptions = input_attr_descs.data();

    std::vector<vk::Format> resolved_color_formats;
    if (!depth_only) {
        if (color_formats.empty()) {
            resolved_color_formats.push_back(swapchain_surface_format.format);
        }
        else {
            resolved_color_formats = color_formats;
        }
    }
    std::vector<vk::PipelineColorBlendAttachmentState> blend_attachments;
    if (depth_only || resolved_color_formats.empty()) {
        local_option.blend_info.attachmentCount = 0;
        local_option.blend_info.pAttachments = nullptr;
    }
    else {
        blend_attachments.assign(resolved_color_formats.size(), local_option.blend_attachment_info);
        local_option.blend_info.attachmentCount = static_cast<uint32_t>(blend_attachments.size());
        local_option.blend_info.pAttachments = blend_attachments.data();
    }

    vk::raii::DescriptorSetLayout descriptor_set_layout{nullptr};
    if (!descriptor_layouts.empty()) {
        vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info{};
        descriptor_set_layout_info.bindingCount = static_cast<uint32_t>(descriptor_layouts.size());
        descriptor_set_layout_info.pBindings = descriptor_layouts.data();
        descriptor_set_layout = vk::raii::DescriptorSetLayout(device, descriptor_set_layout_info);
    }

    vk::DescriptorSetLayout set_layout_handle = descriptor_set_layout != nullptr ? static_cast<vk::DescriptorSetLayout>(*descriptor_set_layout) : nullptr;
    vk::PipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.setLayoutCount = descriptor_set_layout != nullptr ? 1u : 0u;
    pipeline_layout_info.pSetLayouts = descriptor_set_layout != nullptr ? &set_layout_handle : nullptr;
    vk::raii::PipelineLayout pipeline_layout(device, pipeline_layout_info);

    const vk::Format depth_format = depth_only ? find_depth_only_format() : find_depth_format();
    vk::PipelineRenderingCreateInfo rendering_create_info{};
    rendering_create_info.colorAttachmentCount = static_cast<uint32_t>(resolved_color_formats.size());
    rendering_create_info.pColorAttachmentFormats =
        resolved_color_formats.empty() ? nullptr : resolved_color_formats.data();
    rendering_create_info.depthAttachmentFormat = depth_format;
    rendering_create_info.stencilAttachmentFormat = depth_only ? vk::Format::eUndefined : depth_format;
    vk::GraphicsPipelineCreateInfo graphics_pipeline_create_info{};
    graphics_pipeline_create_info.pNext = &rendering_create_info;
    graphics_pipeline_create_info.stageCount = static_cast<uint32_t>(shader_stage_infos.size());
    graphics_pipeline_create_info.pStages = shader_stage_infos.data();
    graphics_pipeline_create_info.pVertexInputState = pipeline_uses_mesh_shader ? nullptr : &local_option.vert_info;
    graphics_pipeline_create_info.pInputAssemblyState = pipeline_uses_mesh_shader ? nullptr : &local_option.assembly_info;
    graphics_pipeline_create_info.pViewportState = &local_option.viewport_info;
    graphics_pipeline_create_info.pRasterizationState = &local_option.raster_info;
    graphics_pipeline_create_info.pMultisampleState = &local_option.multisample_info;
    graphics_pipeline_create_info.pDepthStencilState = &local_option.depth_info;
    graphics_pipeline_create_info.pColorBlendState = &local_option.blend_info;
    graphics_pipeline_create_info.pDynamicState = &local_option.dynamic_info;
    graphics_pipeline_create_info.layout = *pipeline_layout;
    graphics_pipeline_create_info.renderPass = nullptr;
    graphics_pipeline_create_info.subpass = 0;
    vk::raii::Pipelines vk_pipelines(device, nullptr, {graphics_pipeline_create_info});
    if (vk_pipelines.empty()) {
        std::cout << "Pipeline " << name << " creation failed" << std::endl;
        return false;
    }
    vk::raii::Pipeline vk_pipeline = std::move(vk_pipelines.front());

    Pipeline pipeline;
    pipeline.vk_pipeline = std::move(vk_pipeline);
    pipeline.vk_pipeline_layout = std::move(pipeline_layout);
    pipeline.descriptor_set_layout = std::move(descriptor_set_layout);
    pipeline.uses_mesh_shader = pipeline_uses_mesh_shader;
    pipeline.ubos = std::move(pipeline_ubos);
    pipeline.ssbos = std::move(pipeline_ssbos);
    pipeline.sampler_descriptor_counts = std::move(sampler_descriptor_counts);

    const uint32_t swapchain_cnt = static_cast<uint32_t>(swapchain_images.size());
    uint32_t uniform_desc_count = 0;
    for (const auto& [binding, ubo_name] : ubo_binding_to_name) {
        (void)binding;
        const auto ubo_found = pipeline.ubos.find(ubo_name);
        assert(ubo_found != pipeline.ubos.end());
        uniform_desc_count += static_cast<uint32_t>(ubo_found->second.vecsize);
    }
    uint32_t storage_desc_count = 0;
    for (const auto& [binding, ssbo_name] : storage_binding_to_name) {
        (void)binding;
        const auto ssbo_found = pipeline.ssbos.find(ssbo_name);
        assert(ssbo_found != pipeline.ssbos.end());
        storage_desc_count += 1;
    }
    uint32_t image_desc_count = 0;
    for (const auto& [binding, descriptor_count] : pipeline.sampler_descriptor_counts) {
        (void)binding;
        image_desc_count += descriptor_count;
    }

    std::vector<vk::DescriptorPoolSize> pool_sizes;
    if (uniform_desc_count > 0) {
        vk::DescriptorPoolSize pool_size{};
        pool_size.type = vk::DescriptorType::eUniformBuffer;
        pool_size.descriptorCount = uniform_desc_count * swapchain_cnt;
        pool_sizes.push_back(pool_size);
    }
    if (image_desc_count > 0) {
        vk::DescriptorPoolSize pool_size{};
        pool_size.type = vk::DescriptorType::eCombinedImageSampler;
        pool_size.descriptorCount = image_desc_count * swapchain_cnt;
        pool_sizes.push_back(pool_size);
    }
    if (storage_desc_count > 0) {
        vk::DescriptorPoolSize pool_size{};
        pool_size.type = vk::DescriptorType::eStorageBuffer;
        pool_size.descriptorCount = storage_desc_count * swapchain_cnt;
        pool_sizes.push_back(pool_size);
    }

    if (!pool_sizes.empty()) {
        vk::DescriptorPoolCreateInfo descriptor_pool_info{};
        descriptor_pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
        descriptor_pool_info.maxSets = swapchain_cnt;
        descriptor_pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
        descriptor_pool_info.pPoolSizes = pool_sizes.data();
        pipeline.descriptor_pool = vk::raii::DescriptorPool(device, descriptor_pool_info);

        std::vector<vk::DescriptorSetLayout> set_layouts(swapchain_cnt, *pipeline.descriptor_set_layout);
        vk::DescriptorSetAllocateInfo descriptor_set_alloc_info{};
        descriptor_set_alloc_info.descriptorPool = *pipeline.descriptor_pool;
        descriptor_set_alloc_info.descriptorSetCount = swapchain_cnt;
        descriptor_set_alloc_info.pSetLayouts = set_layouts.data();
        pipeline.descriptor_sets = vk::raii::DescriptorSets(device, descriptor_set_alloc_info);

        for (uint32_t i = 0; i < swapchain_cnt; ++i) {
            std::vector<vk::DescriptorBufferInfo> buffer_infos;
            std::vector<vk::DescriptorImageInfo> image_infos;
            std::vector<vk::WriteDescriptorSet> writes;
            buffer_infos.reserve(ubo_binding_to_name.size() + storage_binding_to_name.size());
            image_infos.reserve(pipeline.sampler_descriptor_counts.size());
            writes.reserve(ubo_binding_to_name.size() + storage_binding_to_name.size()
                + pipeline.sampler_descriptor_counts.size());

            for (const auto& [binding, ubo_name] : ubo_binding_to_name) {
                const auto ubo_found = pipeline.ubos.find(ubo_name);
                assert(ubo_found != pipeline.ubos.end());
                auto& ubo = ubo_found->second;
                vk::DescriptorBufferInfo buffer_info{};
                buffer_info.buffer = *ubo.gpu_bufs[i];
                buffer_info.offset = 0;
                buffer_info.range = ubo.size * ubo.vecsize;
                buffer_infos.push_back(buffer_info);
                vk::WriteDescriptorSet write{};
                write.dstSet = *pipeline.descriptor_sets[i];
                write.dstBinding = binding;
                write.descriptorCount = 1;
                write.descriptorType = vk::DescriptorType::eUniformBuffer;
                write.pBufferInfo = &buffer_infos.back();
                writes.push_back(write);
            }

            for (const auto& [binding, ssbo_name] : storage_binding_to_name) {
                const auto ssbo_found = pipeline.ssbos.find(ssbo_name);
                assert(ssbo_found != pipeline.ssbos.end());
                const auto& ssbo = ssbo_found->second;
                if (ssbo.uses_borrowed_descriptors) {
                    if (i >= ssbo.borrowed_descriptors.size()) {
                        continue;
                    }
                    buffer_infos.push_back(ssbo.borrowed_descriptors[i]);
                }
                else if (i < ssbo.gpu_bufs.size()) {
                    vk::DescriptorBufferInfo buffer_info{};
                    buffer_info.buffer = *ssbo.gpu_bufs[i];
                    buffer_info.offset = 0;
                    buffer_info.range = ssbo.size * ssbo.vecsize;
                    buffer_infos.push_back(buffer_info);
                }
                else {
                    continue;
                }
                vk::WriteDescriptorSet write{};
                write.dstSet = *pipeline.descriptor_sets[i];
                write.dstBinding = binding;
                write.descriptorCount = 1;
                write.descriptorType = vk::DescriptorType::eStorageBuffer;
                write.pBufferInfo = &buffer_infos.back();
                writes.push_back(write);
            }

            for (const auto& [binding, tex_name] : tex_binding_to_name) {
                const auto tex_found = textures.find(tex_name);
                if (tex_found == textures.end()) {
                    continue;
                }
                image_infos.push_back(tex_found->second.descriptor);
                vk::WriteDescriptorSet write{};
                write.dstSet = *pipeline.descriptor_sets[i];
                write.dstBinding = binding;
                write.descriptorCount = 1;
                write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
                write.pImageInfo = &image_infos.back();
                writes.push_back(write);
            }

            if (!writes.empty()) {
                device.updateDescriptorSets(writes, {});
            }
        }
    }

    pipelines.emplace(name, std::move(pipeline));
    return true;
}

bool Context::load_compute_pipeline(const std::string& name, const fs::path& path) {
    ComputeShader shader;
    if (!shader.load(path)) {
        return false;
    }
    return create_compute_pipeline(name, shader);
}

bool Context::create_compute_pipeline(const std::string& name, const ComputeShader& shader) {
    if (compute_pipelines.find(name) != compute_pipelines.end()) {
        std::cout << "Compute pipeline " << name << " already exists" << std::endl;
        return false;
    }
    if (shader.spirv_code.empty()) {
        std::cout << "Compute shader for " << name << " is empty" << std::endl;
        return false;
    }

    std::vector<vk::DescriptorSetLayoutBinding> descriptor_layouts;
    descriptor_layouts.reserve(shader.bindings.size());
    std::map<uint32_t, std::string> ubo_binding_to_name;
    std::map<uint32_t, std::string> ssbo_binding_to_name;
    std::map<uint32_t, std::string> tex_binding_to_name;
    std::unordered_map<std::string, UBO> compute_ubos;
    std::unordered_map<std::string, SSBO> compute_ssbos;

    for (const auto& binding : shader.bindings) {
        if (binding.kind == ComputeDescriptorKind::StorageImage)
        {
            std::cout << "Compute descriptor kind for binding " << binding.name
                << " is not yet wired in Context resource manager" << std::endl;
            return false;
        }

        if (binding.kind == ComputeDescriptorKind::UniformBuffer) {
            const auto ubo_name = name + ":" + binding.name;
            const uint32_t struct_size = binding.struct_size == 0 ? 16u : binding.struct_size;
            add_ubo(compute_ubos, ubo_name, binding.binding, struct_size, binding.descriptor_count);
            ubo_binding_to_name[binding.binding] = ubo_name;
        }
        else if (binding.kind == ComputeDescriptorKind::StorageBuffer) {
            const auto ssbo_name = name + ":" + binding.name;
            SSBO ssbo{};
            ssbo.size = binding.struct_size == 0 ? 16u : binding.struct_size;
            ssbo.vecsize = 0;
            ssbo.binding = binding.binding;
            ssbo.descriptor_type = vk::DescriptorType::eStorageBuffer;
            compute_ssbos.emplace(ssbo_name, std::move(ssbo));
            ssbo_binding_to_name[binding.binding] = ssbo_name;
        }
        else if (binding.kind == ComputeDescriptorKind::CombinedImageSampler) {
            const auto tex_name = name + ":" + binding.name;
            tex_binding_to_name[binding.binding] = tex_name;
        }

        vk::DescriptorSetLayoutBinding layout_binding{};
        layout_binding.binding = binding.binding;
        layout_binding.descriptorType = to_vk_descriptor_type(binding.kind);
        layout_binding.descriptorCount = binding.descriptor_count;
        layout_binding.stageFlags = vk::ShaderStageFlagBits::eCompute;
        descriptor_layouts.push_back(layout_binding);
    }

    vk::raii::ShaderModule shader_module{nullptr};
    {
        vk::ShaderModuleCreateInfo shader_module_create_info{};
        shader_module_create_info.codeSize = shader.spirv_code.size() * sizeof(uint32_t);
        shader_module_create_info.pCode = shader.spirv_code.data();
        shader_module = vk::raii::ShaderModule(device, shader_module_create_info);
    }

    vk::raii::DescriptorSetLayout descriptor_set_layout{nullptr};
    if (!descriptor_layouts.empty()) {
        vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info{};
        descriptor_set_layout_info.bindingCount = static_cast<uint32_t>(descriptor_layouts.size());
        descriptor_set_layout_info.pBindings = descriptor_layouts.data();
        descriptor_set_layout = vk::raii::DescriptorSetLayout(device, descriptor_set_layout_info);
    }

    vk::DescriptorSetLayout set_layout_handle = descriptor_set_layout != nullptr
        ? static_cast<vk::DescriptorSetLayout>(*descriptor_set_layout)
        : nullptr;
    vk::PipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.setLayoutCount = descriptor_set_layout != nullptr ? 1u : 0u;
    pipeline_layout_info.pSetLayouts = descriptor_set_layout != nullptr ? &set_layout_handle : nullptr;
    vk::raii::PipelineLayout pipeline_layout(device, pipeline_layout_info);

    vk::PipelineShaderStageCreateInfo shader_stage_info{};
    shader_stage_info.stage = vk::ShaderStageFlagBits::eCompute;
    shader_stage_info.module = *shader_module;
    shader_stage_info.pName = "main";

    vk::ComputePipelineCreateInfo compute_pipeline_create_info{};
    compute_pipeline_create_info.stage = shader_stage_info;
    compute_pipeline_create_info.layout = *pipeline_layout;
    vk::raii::Pipeline compute_pipeline(device, nullptr, compute_pipeline_create_info);

    ComputePipeline pipeline{};
    pipeline.vk_pipeline = std::move(compute_pipeline);
    pipeline.vk_pipeline_layout = std::move(pipeline_layout);
    pipeline.descriptor_set_layout = std::move(descriptor_set_layout);
    pipeline.ubos = std::move(compute_ubos);
    pipeline.ssbos = std::move(compute_ssbos);

    const uint32_t swapchain_cnt = static_cast<uint32_t>(swapchain_images.size());
    std::unordered_map<vk::DescriptorType, uint32_t> descriptor_type_counts;
    for (const auto& binding : shader.bindings) {
        const auto descriptor_type = to_vk_descriptor_type(binding.kind);
        descriptor_type_counts[descriptor_type] += binding.descriptor_count * swapchain_cnt;
    }
    std::vector<vk::DescriptorPoolSize> pool_sizes;
    pool_sizes.reserve(descriptor_type_counts.size());
    for (const auto& [descriptor_type, descriptor_count] : descriptor_type_counts) {
        vk::DescriptorPoolSize pool_size{};
        pool_size.type = descriptor_type;
        pool_size.descriptorCount = descriptor_count;
        pool_sizes.push_back(pool_size);
    }

    if (!pool_sizes.empty()) {
        vk::DescriptorPoolCreateInfo descriptor_pool_info{};
        descriptor_pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
        descriptor_pool_info.maxSets = swapchain_cnt;
        descriptor_pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
        descriptor_pool_info.pPoolSizes = pool_sizes.data();
        pipeline.descriptor_pool = vk::raii::DescriptorPool(device, descriptor_pool_info);

        std::vector<vk::DescriptorSetLayout> set_layouts(swapchain_cnt, *pipeline.descriptor_set_layout);
        vk::DescriptorSetAllocateInfo descriptor_set_alloc_info{};
        descriptor_set_alloc_info.descriptorPool = *pipeline.descriptor_pool;
        descriptor_set_alloc_info.descriptorSetCount = swapchain_cnt;
        descriptor_set_alloc_info.pSetLayouts = set_layouts.data();
        pipeline.descriptor_sets = vk::raii::DescriptorSets(device, descriptor_set_alloc_info);

        for (uint32_t i = 0; i < swapchain_cnt; ++i) {
            std::vector<vk::DescriptorBufferInfo> buffer_infos;
            std::vector<vk::DescriptorImageInfo> image_infos;
            std::vector<vk::WriteDescriptorSet> writes;
            buffer_infos.reserve(ubo_binding_to_name.size() + ssbo_binding_to_name.size());
            image_infos.reserve(tex_binding_to_name.size());
            writes.reserve(ubo_binding_to_name.size() + ssbo_binding_to_name.size() + tex_binding_to_name.size());

            for (const auto& [binding, ubo_name] : ubo_binding_to_name) {
                const auto ubo_found = pipeline.ubos.find(ubo_name);
                if (ubo_found == pipeline.ubos.end()) {
                    continue;
                }
                auto& ubo = ubo_found->second;
                vk::DescriptorBufferInfo buffer_info{};
                buffer_info.buffer = *ubo.gpu_bufs[i];
                buffer_info.offset = 0;
                buffer_info.range = ubo.size * ubo.vecsize;
                buffer_infos.push_back(buffer_info);

                vk::WriteDescriptorSet write{};
                write.dstSet = *pipeline.descriptor_sets[i];
                write.dstBinding = binding;
                write.descriptorCount = 1;
                write.descriptorType = vk::DescriptorType::eUniformBuffer;
                write.pBufferInfo = &buffer_infos.back();
                writes.push_back(write);
            }

            // Compute SSBOs are allocated explicitly after the caller sets capacity.
            for (const auto& [binding, tex_name] : tex_binding_to_name) {
                const auto tex_found = textures.find(tex_name);
                if (tex_found == textures.end()) {
                    std::cout << "Compute texture " << tex_name << " not found for pipeline " << name << std::endl;
                    return false;
                }
                image_infos.push_back(tex_found->second.descriptor);

                vk::WriteDescriptorSet write{};
                write.dstSet = *pipeline.descriptor_sets[i];
                write.dstBinding = binding;
                write.descriptorCount = 1;
                write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
                write.pImageInfo = &image_infos.back();
                writes.push_back(write);
            }

            if (!writes.empty()) {
                device.updateDescriptorSets(writes, {});
            }
        }
    }

    compute_pipelines.emplace(name, std::move(pipeline));
    return true;
}

} // namespace vkkk
