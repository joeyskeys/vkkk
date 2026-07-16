#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv.hpp>
#include <spirv_cross/spirv_glsl.hpp>

#include "utils/io.h"
#include "vk_ins/compute_shader.hpp"

namespace vkkk
{

namespace
{

uint32_t reflect_array_size(const spirv_cross::SPIRType& type_info) {
    return type_info.array.empty() ? 1u : type_info.array[0];
}

bool reflect_compute_shader(ComputeShader& shader) {
    shader.bindings.clear();
    shader.local_size = {1, 1, 1};

    spirv_cross::CompilerGLSL comp(shader.spirv_code);
    const auto resources = comp.get_shader_resources();

    auto append_buffer_binding = [&](const spirv_cross::Resource& resource, ComputeDescriptorKind kind) {
        const auto binding = comp.get_decoration(resource.id, spv::DecorationBinding);
        const auto type_info = comp.get_type(resource.type_id);
        const auto base_type_info = comp.get_type(resource.base_type_id);
        uint32_t struct_size = static_cast<uint32_t>(comp.get_declared_struct_size(base_type_info));
        if (kind == ComputeDescriptorKind::StorageBuffer && struct_size == 0
            && !base_type_info.member_types.empty())
        {
            struct_size = static_cast<uint32_t>(comp.type_struct_member_array_stride(base_type_info, 0));
            if (struct_size == 0) {
                const auto member_type = comp.get_type(base_type_info.member_types[0]);
                if (!member_type.member_types.empty()) {
                    struct_size = static_cast<uint32_t>(comp.get_declared_struct_size(member_type));
                }
            }
        }
        std::string type_name = comp.get_name(resource.base_type_id);
        if (type_name.empty()) {
            type_name = resource.name;
        }
        shader.bindings.push_back(ComputeDescriptorBinding{
            .name = std::move(type_name),
            .binding = binding,
            .descriptor_count = reflect_array_size(type_info),
            .struct_size = struct_size,
            .kind = kind
        });
    };

    auto append_image_binding = [&](const spirv_cross::Resource& resource, ComputeDescriptorKind kind) {
        const auto binding = comp.get_decoration(resource.id, spv::DecorationBinding);
        const auto type_info = comp.get_type(resource.type_id);
        shader.bindings.push_back(ComputeDescriptorBinding{
            .name = resource.name,
            .binding = binding,
            .descriptor_count = reflect_array_size(type_info),
            .struct_size = 0,
            .kind = kind
        });
    };

    for (const auto& resource : resources.uniform_buffers) {
        append_buffer_binding(resource, ComputeDescriptorKind::UniformBuffer);
    }
    for (const auto& resource : resources.storage_buffers) {
        append_buffer_binding(resource, ComputeDescriptorKind::StorageBuffer);
    }
    for (const auto& resource : resources.sampled_images) {
        append_image_binding(resource, ComputeDescriptorKind::CombinedImageSampler);
    }
    for (const auto& resource : resources.storage_images) {
        append_image_binding(resource, ComputeDescriptorKind::StorageImage);
    }

    std::sort(shader.bindings.begin(), shader.bindings.end(),
        [](const ComputeDescriptorBinding& a, const ComputeDescriptorBinding& b) {
            return a.binding < b.binding;
        });

    try {
        shader.local_size[0] = comp.get_execution_mode_argument(spv::ExecutionModeLocalSize, 0);
        shader.local_size[1] = comp.get_execution_mode_argument(spv::ExecutionModeLocalSize, 1);
        shader.local_size[2] = comp.get_execution_mode_argument(spv::ExecutionModeLocalSize, 2);
    }
    catch (const std::exception&) {
        shader.local_size = {1, 1, 1};
    }

    return true;
}

} // namespace

bool ComputeShader::load(const char* source, const std::string& source_name) {
    spirv_code.clear();
    bindings.clear();

    if (source == nullptr) {
        std::cout << "Compute shader source pointer is null" << std::endl;
        return false;
    }

    source_code.assign(source, source + std::strlen(source));
    if (source_code.empty()) {
        std::cout << "Compute shader source is empty for " << source_name << std::endl;
        return false;
    }

    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetSourceLanguage(shaderc_source_language_glsl);
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
    options.SetForcedVersionProfile(450, shaderc_profile_none);

    const std::string source_text(source_code.begin(), source_code.end());
    shaderc::SpvCompilationResult ret =
        compiler.CompileGlslToSpv(source_text, shaderc_glsl_compute_shader, source_name.c_str(), "main", options);
    if (ret.GetCompilationStatus() != shaderc_compilation_status_success) {
        std::cout << ret.GetErrorMessage();
        return false;
    }

    spirv_code.insert(spirv_code.begin(), ret.cbegin(), ret.cend());
    return reflect_compute_shader(*this);
}

bool ComputeShader::load(const fs::path& path) {
    auto abs_path = ensure_abs_path(path);
    if (!fs::exists(abs_path)) {
        std::cout << "Compute shader file " << abs_path << " does not exist" << std::endl;
        return false;
    }

    if (abs_path.extension().string().ends_with(".spv")) {
        spirv_code = load_spirv_file(abs_path);
        if (spirv_code.empty()) {
            std::cout << "Failed to load SPIR-V compute shader file: " << abs_path << std::endl;
            return false;
        }
        return reflect_compute_shader(*this);
    }

    source_code = load_file(abs_path);
    if (source_code.empty()) {
        std::cout << "Failed to load compute shader source file: " << abs_path << std::endl;
        return false;
    }
    const std::string source_text(source_code.begin(), source_code.end());
    return load(source_text.c_str(), abs_path.filename().string());
}

} // namespace vkkk
