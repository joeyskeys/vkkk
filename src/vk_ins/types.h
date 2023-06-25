#include <array>

#include <vulkan/vulkan.h>

namespace vkkk
{

enum GLSLTYPE {
    UNKNOW,
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

constexpr static std::array<VK_Format, 7> glsl_type_macro = {
    VK_FORMAT_R32_SFLOAT,
    VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT,
    VK_FORMAT_R32G32_SINT, VK_FORMAT_R32G32B32_SINT, VK_FORMAT_R32G32B32A32_SINT
};

}