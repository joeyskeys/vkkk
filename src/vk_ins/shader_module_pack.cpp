#include <fmt/format.h>
#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv.hpp>
#include <spirv_cross/spirv_glsl.hpp>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>

#include "utils/io.h"
#include "vk_ins/shader_module_pack.hpp"

namespace vkkk
{

namespace
{

constexpr std::uint32_t kSpirvMagic = 0x07230203u;
constexpr std::string_view kSpirvCacheSignature = "vkkk-spirv-cache-v1";

bool valid_spirv(const std::vector<std::uint32_t>& spirv) {
    return !spirv.empty() && spirv.front() == kSpirvMagic;
}

std::uint64_t fnv1a_append(std::uint64_t hash, std::string_view value) {
    for (const unsigned char byte : value) {
        hash ^= byte;
        hash *= 1099511628211ull;
    }
    return hash;
}

} // namespace

fs::path spirv_cache_path(std::string_view source,
    vk::ShaderStageFlagBits stage, const ShaderCacheOptions& options)
{
    if (!options.enabled()) {
        return {};
    }

    auto hash = fnv1a_append(1469598103934665603ull,
        kSpirvCacheSignature);
    hash = fnv1a_append(hash, source);
    const auto stage_value = static_cast<std::uint32_t>(stage);
    for (std::size_t i = 0; i < sizeof(stage_value); ++i) {
        hash ^= static_cast<unsigned char>(stage_value >> (i * 8));
        hash *= 1099511628211ull;
    }

    std::ostringstream name;
    name << "shader_" << std::hex << std::setw(16)
         << std::setfill('0') << hash << ".spv";
    return options.directory / name.str();
}

bool load_spirv_cache(const fs::path& path, std::vector<uint32_t>& spirv) {
    spirv.clear();
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return false;
    }

    const auto end = file.tellg();
    if (end <= 0
        || static_cast<std::uintmax_t>(end) % sizeof(std::uint32_t) != 0)
    {
        return false;
    }
    spirv.resize(static_cast<std::size_t>(end) / sizeof(std::uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(spirv.data()),
        static_cast<std::streamsize>(
            spirv.size() * sizeof(std::uint32_t)));
    if (!file || !valid_spirv(spirv)) {
        spirv.clear();
        return false;
    }
    return true;
}

bool save_spirv_cache(const fs::path& path,
    const std::vector<uint32_t>& spirv)
{
    if (path.empty() || !valid_spirv(spirv)) {
        return false;
    }

    std::error_code error;
    if (!path.parent_path().empty()) {
        fs::create_directories(path.parent_path(), error);
        if (error) {
            return false;
        }
    }

    const fs::path temporary = path.string() + ".tmp";
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file) {
            return false;
        }
        file.write(reinterpret_cast<const char*>(spirv.data()),
            static_cast<std::streamsize>(
                spirv.size() * sizeof(std::uint32_t)));
        file.flush();
        if (!file) {
            file.close();
            fs::remove(temporary, error);
            return false;
        }
    }

    fs::remove(path, error);
    error.clear();
    fs::rename(temporary, path, error);
    if (error) {
        fs::remove(temporary, error);
        return false;
    }
    return true;
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

static bool reflect_shader_module(ShaderModule& mod, const vk::ShaderStageFlagBits t) {
    mod.buf_infos.clear();
    mod.storage_buf_infos.clear();
    mod.img_infos.clear();
    mod.attr_infos.clear();
    mod.push_constant_infos.clear();

    spirv_cross::CompilerGLSL comp(mod.spirv_code);
    auto res = comp.get_shader_resources();

    // UBOs — key by block/type name so instance names (camera, ubo, …) do not matter.
    for (auto& ubo : res.uniform_buffers) {
        auto name = comp.get_name(ubo.base_type_id);
        if (name.empty())
            name = ubo.name;
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

    // Storage buffers — key by block/type name (not instance name).
    for (auto& ssbo : res.storage_buffers) {
        auto name = comp.get_name(ssbo.base_type_id);
        if (name.empty())
            name = ssbo.name;
        if (name.empty())
            name = fmt::format("ssbo_{}", comp.get_decoration(ssbo.id, spv::DecorationBinding));
        auto type_info = comp.get_type(ssbo.type_id);
        auto base_type_info = comp.get_type(ssbo.base_type_id);
        auto binding_idx = comp.get_decoration(ssbo.id, spv::DecorationBinding);
        auto struct_size = static_cast<uint32_t>(comp.get_declared_struct_size(base_type_info));
        auto array_size = type_info.array.size() > 0 ? type_info.array[0] : 1;

        // Runtime-sized SSBO blocks report block size 0. Prefer array stride so scalar/vector
        // members (e.g. uint[], uvec4[]) work; get_declared_struct_size only accepts structs.
        if (struct_size == 0 && !base_type_info.member_types.empty()) {
            struct_size = static_cast<uint32_t>(
                comp.type_struct_member_array_stride(base_type_info, 0));
            const auto member_type = comp.get_type(base_type_info.member_types[0]);
            if (!member_type.member_types.empty()) {
                // Nested struct member (e.g. MainDirectionalShadowData light;): use the
                // declared struct size. Array stride can be 0/wrong for non-array members
                // and would fall through to the 16-byte default in create_pipeline.
                const auto nested_size =
                    static_cast<uint32_t>(comp.get_declared_struct_size(member_type));
                if (nested_size > 0) {
                    struct_size = nested_size;
                }
            }
            if (!member_type.array.empty()) {
                // Runtime arrays report 0; capacity is set later by the user.
                array_size = member_type.array[0];
            }
        }
        mod.storage_buf_infos.emplace(name, std::make_tuple(struct_size, array_size, binding_idx));
    }

    // Push constants — key by block/type name; offset is the first used member offset.
    for (auto& pc : res.push_constant_buffers) {
        auto name = comp.get_name(pc.base_type_id);
        if (name.empty())
            name = pc.name;
        if (name.empty())
            name = "PushConstants";
        const auto base_type_info = comp.get_type(pc.base_type_id);
        auto struct_size = static_cast<uint32_t>(comp.get_declared_struct_size(base_type_info));
        uint32_t offset = 0;
        const auto ranges = comp.get_active_buffer_ranges(pc.id);
        if (!ranges.empty()) {
            offset = static_cast<uint32_t>(ranges.front().offset);
            uint32_t end = 0;
            for (const auto& range : ranges) {
                end = std::max(end, static_cast<uint32_t>(range.offset + range.range));
            }
            if (end > offset) {
                struct_size = end - offset;
            }
        }
        if (struct_size == 0) {
            continue;
        }
        mod.push_constant_infos.emplace(name, std::make_tuple(struct_size, offset));
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

        case vk::ShaderStageFlagBits::eMeshEXT: {
            out_kind = shaderc_glsl_mesh_shader;
            return true;
        }

        case vk::ShaderStageFlagBits::eTaskEXT: {
            out_kind = shaderc_glsl_task_shader;
            return true;
        }

        default: {
            return false;
        }
    }
}

bool ShaderModule::load(const char* source, const vk::ShaderStageFlagBits t,
    const std::string& source_name, const ShaderCacheOptions& cache)
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

    const std::string source_text(source_code.begin(), source_code.end());
    if (cache.enabled() && !cache.force_recompile) {
        const auto cache_file = spirv_cache_path(source_text, t, cache);
        if (load_spirv_cache(cache_file, spirv_code)) {
            try {
                if (reflect_shader_module(*this, t)) {
                    return true;
                }
            }
            catch (const std::exception&) {
                // Recompile if this cache was produced by an incompatible
                // reflection toolchain.
            }
            spirv_code.clear();
        }
    }

    shaderc_shader_kind tt;
    if (!shader_kind_from_stage(t, tt)) {
        std::cout << "Shader type " << static_cast<uint32_t>(t) << " not supported yet.." << std::endl;
        return false;
    }

    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetSourceLanguage(shaderc_source_language_glsl);
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
    options.SetForcedVersionProfile(450, shaderc_profile_none);

    shaderc::SpvCompilationResult ret =
        compiler.CompileGlslToSpv(source_text, tt, source_name.c_str(), "main", options);

    if (ret.GetCompilationStatus() != shaderc_compilation_status_success) {
        std::cout << ret.GetErrorMessage();
        return false;
    }

    spirv_code.insert(spirv_code.begin(), ret.cbegin(), ret.cend());
    if (cache.enabled()) {
        save_spirv_cache(spirv_cache_path(source_text, t, cache), spirv_code);
    }
    return reflect_shader_module(*this, t);
}

bool ShaderModule::load(const fs::path& path,
    const vk::ShaderStageFlagBits t, const ShaderCacheOptions& cache)
{
    type = t;

    auto abs_path = ensure_abs_path(path);
    if (!fs::exists(abs_path)) {
        std::cout << "Shader file " << abs_path << " does not exist" << std::endl;
        return false;
    }
    auto extension = abs_path.extension();

    if (extension.string().ends_with(".spv")) {
        return load_spirv(abs_path, t);
    }

    source_code = load_file(abs_path);
    if (source_code.empty()) {
        std::cout << "Failed to load shader source file: " << abs_path << std::endl;
        return false;
    }

    const std::string source_text(source_code.begin(), source_code.end());
    return load(source_text.c_str(), t, abs_path.filename().string(), cache);
}

bool ShaderModule::load_spirv(const fs::path& path,
    const vk::ShaderStageFlagBits t)
{
    type = t;
    source_code.clear();
    if (!load_spirv_cache(path, spirv_code)) {
        std::cout << "Failed to load SPIR-V file: " << path << std::endl;
        return false;
    }
    return reflect_shader_module(*this, t);
}

bool ShaderModule::save_spirv(const fs::path& path) const {
    return save_spirv_cache(path, spirv_code);
}

bool ShaderModulePack::add_shader_module(const ShaderModule& module, bool replace) {
    const bool is_vertex_stage = module.type == vk::ShaderStageFlagBits::eVertex;
    const bool is_mesh_stage = module.type == vk::ShaderStageFlagBits::eMeshEXT
        || module.type == vk::ShaderStageFlagBits::eTaskEXT;

    if (is_mesh_stage && modules.contains(vk::ShaderStageFlagBits::eVertex)) {
        std::cout << "Cannot mix mesh/task shader with vertex shader in one pipeline pack." << std::endl;
        return false;
    }
    if (is_vertex_stage && (modules.contains(vk::ShaderStageFlagBits::eMeshEXT)
        || modules.contains(vk::ShaderStageFlagBits::eTaskEXT)))
    {
        std::cout << "Cannot mix vertex shader with mesh/task shader in one pipeline pack." << std::endl;
        return false;
    }

    if (modules.find(module.type) != modules.end() && !replace) {
        std::cout << "Shader module for stage " << static_cast<uint32_t>(module.type) << " already exists" << std::endl;
        return false;
    }
    modules[module.type] = module;
    if (is_mesh_stage) {
        use_mesh_shader = true;
    }
    return true;
}

}