#pragma once

#include <array>
#include <vector>

#include <vulkan/vulkan.h>

namespace vkkk
{

enum GLSLTYPE {
    UNKNOWN,
    VKKK_VEC2F,
    VKKK_VEC3F,
    VKKK_VEC4F,
    VKKK_VEC2I,
    VKKK_VEC3I,
    VKKK_VEC4I
};

constexpr static std::array<uint32_t, 7> glsl_type_sizes = {
    0, 8, 12, 16, 8, 12, 16
};

constexpr static std::array<VkFormat, 7> glsl_type_macro = {
    VK_FORMAT_R32_SFLOAT,
    VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT,
    VK_FORMAT_R32G32_SINT, VK_FORMAT_R32G32B32_SINT, VK_FORMAT_R32G32B32A32_SINT
};

enum VERT_COMP {
    VERTEX,
    NORMAL,
    UV,
    COLOR
};

static std::array<uint32_t, 4> comp_sizes = {
    3, 3, 2, 3
};

inline uint32_t get_mesh_component_size(const std::vector<VERT_COMP>& comps) {
    uint32_t size = 0;
    for (const auto& comp : comps) {
        size += comp_sizes[comp];
    }
    return size;
}

}