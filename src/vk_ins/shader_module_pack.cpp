#include <fmt/format.h>
#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv.hpp>
#include <spirv_cross/spirv_glsl.hpp>
#include <algorithm>
#include <cstring>

#include "utils/io.h"
#include "vk_ins/shader_module_pack.hpp"

namespace vkkk
{

static GLSLTYPE find_vec_type(spirv_cross::SPIRType t) {
    enum GLSLTYPE vt = GLSLTYPE::UNKNOWN;
    assert(t.vecsize > 1);
    if (t.basetype == spirv_cross::SPIRType::Float)
        vt = static_cast<GLSLTYPE>(t.vecsize - 1);
    else if (t.basetype == spirv_cross::SPIRType::Int)
        vt = static_cast<GLSLTYPE>(2 + t.vecsize);
    return vt;
}

static bool reflect_shader_module(ShaderModule& mod, const vk::ShaderStageFlagBits t) {
    mod.buf_infos.clear();
    mod.img_infos.clear();
    mod.attr_infos.clear();

    spirv_cross::CompilerGLSL comp(mod.spirv_code);
    auto res = comp.get_shader_resources();

    // UBOs
    for (auto& ubo : res.uniform_buffers) {
        auto name = comp.get_name(ubo.id);
        if (name.empty())
            name = comp.get_name(ubo.base_type_id);
        if (name.empty())
            name = fmt::format("ubo_{}", comp.get_decoration(ubo.id, spv::DecorationBinding));
        auto type_info = comp.get_type(ubo.type_id);
        auto base_type_info = comp.get_type(ubo.base_type_id);
        auto binding_idx = comp.get_decoration(ubo.id, spv::DecorationBinding);
        auto struct_size = static_cast<uint32_t>(comp.get_declared_struct_size(base_type_info));
        // This code is problematic, we're assuming the array always be 1 dimension
        auto array_size = type_info.array.size() > 0 ? type_info.array[0] : 1;
        mod.buf_infos.emplace(name, std::make_tuple(struct_size, array_size, binding_idx));
    }

    // Textures
    for (auto& img : res.sampled_images) {
        auto binding_idx = comp.get_decoration(img.id, spv::DecorationBinding);
        mod.img_infos.emplace(img.name, binding_idx);
    }

    // input attrs
    if (t == vk::ShaderStageFlagBits::eVertex) {
        for (auto& input : res.stage_inputs) {
            auto name = comp.get_name(input.id);
            auto type_info = comp.get_type(input.base_type_id);
            auto vectype = find_vec_type(type_info);
            auto loc = comp.get_decoration(input.id, spv::DecorationLocation);
            mod.attr_infos.emplace_back(loc, static_cast<uint32_t>(vectype), name);
        }
        // sort by location index
        std::sort(mod.attr_infos.begin(), mod.attr_infos.end(), [](const auto& a, const auto& b) {
            return std::get<0>(a) < std::get<0>(b);
        });
    }

    return true;
}

static bool shader_kind_from_stage(const vk::ShaderStageFlagBits t, shaderc_shader_kind& out_kind) {
    switch (t) {
        case vk::ShaderStageFlagBits::eVertex: {
            out_kind = shaderc_glsl_vertex_shader;
            return true;
        }

        case vk::ShaderStageFlagBits::eTessellationControl: {
            out_kind = shaderc_glsl_tess_control_shader;
            return true;
        }

        case vk::ShaderStageFlagBits::eTessellationEvaluation: {
            out_kind = shaderc_glsl_tess_evaluation_shader;
            return true;
        }

        case vk::ShaderStageFlagBits::eGeometry: {
            out_kind = shaderc_glsl_geometry_shader;
            return true;
        }

        case vk::ShaderStageFlagBits::eFragment: {
            out_kind = shaderc_glsl_fragment_shader;
            return true;
        }

        case vk::ShaderStageFlagBits::eCompute: {
            out_kind = shaderc_glsl_compute_shader;
            return true;
        }

        default: {
            return false;
        }
    }
}

bool ShaderModule::load(const char* source, const vk::ShaderStageFlagBits t,
    const std::string& source_name)
{
    type = t;
    spirv_code.clear();

    if (source == nullptr) {
        std::cout << "Shader source pointer is null" << std::endl;
        return false;
    }

    source_code.assign(source, source + std::strlen(source));
    if (source_code.empty()) {
        std::cout << "Shader source is empty for " << source_name << std::endl;
        return false;
    }

    shaderc_shader_kind tt;
    if (!shader_kind_from_stage(t, tt)) {
        std::cout << "Shader type " << static_cast<uint32_t>(t) << " not supported yet.." << std::endl;
        return false;
    }

    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetSourceLanguage(shaderc_source_language_glsl);
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_0);
    options.SetForcedVersionProfile(450, shaderc_profile_none);

    const std::string source_text(source_code.begin(), source_code.end());
    shaderc::SpvCompilationResult ret =
        compiler.CompileGlslToSpv(source_text, tt, source_name.c_str(), "main", options);

    if (ret.GetCompilationStatus() != shaderc_compilation_status_success) {
        std::cout << ret.GetErrorMessage();
        return false;
    }

    spirv_code.insert(spirv_code.begin(), ret.cbegin(), ret.cend());
    return reflect_shader_module(*this, t);
}

bool ShaderModule::load(const fs::path& path, const vk::ShaderStageFlagBits t) {
    type = t;

    auto abs_path = ensure_abs_path(path);
    if (!fs::exists(abs_path)) {
        std::cout << "Shader file " << abs_path << " does not exist" << std::endl;
        return false;
    }
    auto extension = abs_path.extension();

    if (extension.string().ends_with(".spv")) {
        // Compiled SPRIV
        spirv_code = load_spirv_file(abs_path);
        if (spirv_code.empty()) {
            std::cout << "Failed to load SPIR-V file: " << abs_path << std::endl;
            return false;
        }
        return reflect_shader_module(*this, t);
    }

    source_code = load_file(abs_path);
    if (source_code.empty()) {
        std::cout << "Failed to load shader source file: " << abs_path << std::endl;
        return false;
    }

    const std::string source_text(source_code.begin(), source_code.end());
    return load(source_text.c_str(), t, abs_path.filename().string());
}

bool ShaderModulePack::add_shader_module(const ShaderModule& module, bool replace) {
    if (modules.find(module.type) != modules.end() && !replace) {
        std::cout << "Shader module for stage " << static_cast<uint32_t>(module.type) << " already exists" << std::endl;
        return false;
    }
    modules[module.type] = module;
    return true;
}

}