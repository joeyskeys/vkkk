#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "utils/sizeable.hpp"
#include "built_in_shader/common.h"

namespace vkkk
{

struct FixedColorInstanceAttrs: public Sizeable<FixedColorUBO> {
    glm::mat4 model{1.0f};
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
};

inline constexpr const char fixed_color_vert[] = R"(
#version 460

layout(binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} ubo;

layout(std430, binding = 2) readonly buffer FixedColorInstanceAttrs {
    mat4 model[16];
    vec4 color[16];
} instance_attrs;

layout(location = 0) in vec3 inPosition;
layout(location = 0) out vec4 fragColor;

void main() {
    gl_Position = ubo.proj * ubo.view * instance_attrs.model[gl_InstanceIndex] * vec4(inPosition, 1.0);
    fragColor = instance_attrs.color[gl_InstanceIndex];
}
)";

inline constexpr const char fixed_color_frag[] = R"(
#version 450

layout(binding = 1) uniform FixedColor {
    vec4 color;
} fixed_color;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = fixed_color.color;
}
)";

namespace built_in_shader
{
using FixedColorTransformUBO = ::vkkk::CameraUBO;
using FixedColorUBO = ::vkkk::FixedColorUBO;
using FixedColorDrawModeUBO = ::vkkk::FixedColorDrawModeUBO;
inline constexpr auto& fixed_color_vert = ::vkkk::fixed_color_vert;
inline constexpr auto& fixed_color_frag = ::vkkk::fixed_color_frag;
} // namespace built_in_shader

} // namespace vkkk
