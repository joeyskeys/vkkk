#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace vkkk::built_in_shader
{

struct FixedColorTransformUBO {
    glm::mat4 model{1.0f};
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};
};

struct FixedColorUBO {
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
};

inline constexpr const char fixed_color_vert[] = R"(
#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
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

} // namespace vkkk::built_in_shader
