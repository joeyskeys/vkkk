#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "vk_ins/shader_module_pack.hpp"

namespace fs = std::filesystem;

namespace vkkk
{

enum class ComputeDescriptorKind : uint32_t {
    UniformBuffer,
    StorageBuffer,
    CombinedImageSampler,
    StorageImage
};

struct ComputeDescriptorBinding {
    std::string name;
    uint32_t binding = 0;
    uint32_t descriptor_count = 1;
    uint32_t struct_size = 0;
    ComputeDescriptorKind kind = ComputeDescriptorKind::UniformBuffer;
};

class ComputeShader {
public:
    std::vector<char> source_code;
    std::vector<uint32_t> spirv_code;
    std::vector<ComputeDescriptorBinding> bindings;
    std::array<uint32_t, 3> local_size{1, 1, 1};

    bool load(const char* source, const std::string& source_name = "inline_compute_shader");
    bool load(const fs::path& path);
};

} // namespace vkkk
