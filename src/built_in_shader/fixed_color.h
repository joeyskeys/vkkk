#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "utils/sizeable.hpp"
#include "built_in_shader/common.h"

namespace vkkk
{

struct FixedColorTransformUBO : public Sizeable<FixedColorTransformUBO> {
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};

    size_t size() const override {
        return sizeof(FixedColorTransformUBO);
    }
};

struct FixedColorUBO : public Sizeable<FixedColorUBO> {
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};

    size_t size() const override {
        return sizeof(FixedColorUBO);
    }
};

// draw_mode.x: 0=wireframe, 1=shaded, 2=shaded_wireframe
struct FixedColorDrawModeUBO : public Sizeable<FixedColorDrawModeUBO> {
    glm::ivec4 draw_mode{1, 0, 0, 0};

    size_t size() const override {
        return sizeof(FixedColorDrawModeUBO);
    }
};

struct FixedColorMeshVerticesSSBO : public Sizeable<FixedColorMeshVerticesSSBO> {
    glm::vec4 positions[3] = {
        glm::vec4(-0.5f, -0.5f, 0.0f, 1.0f),
        glm::vec4(0.5f, -0.5f, 0.0f, 1.0f),
        glm::vec4(0.0f, 0.5f, 0.0f, 1.0f)
    };

    size_t size() const override {
        return sizeof(FixedColorMeshVerticesSSBO);
    }
};

struct FixedColorMeshIndicesSSBO : public Sizeable<FixedColorMeshIndicesSSBO> {
    glm::uvec4 triangles[1] = {
        glm::uvec4(0u, 1u, 2u, 0u)
    };

    size_t size() const override {
        return sizeof(FixedColorMeshIndicesSSBO);
    }
};

inline constexpr char fixed_color_vert[] = R"(
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

inline constexpr const char fixed_color_vert_instanced[] = R"(
#version 460

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(std430, binding = 2) readonly buffer InstanceAttrs {
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
using FixedColorTransformUBO = ::vkkk::FixedColorTransformUBO;
using FixedColorUBO = ::vkkk::FixedColorUBO;
using FixedColorDrawModeUBO = ::vkkk::FixedColorDrawModeUBO;
using FixedColorMeshVerticesSSBO = ::vkkk::FixedColorMeshVerticesSSBO;
using FixedColorMeshIndicesSSBO = ::vkkk::FixedColorMeshIndicesSSBO;
inline constexpr auto& fixed_color_vert = ::vkkk::fixed_color_vert;
inline constexpr auto& fixed_color_vert_instanced = ::vkkk::fixed_color_vert_instanced;
inline constexpr auto& fixed_color_mesh = ::vkkk::fixed_color_mesh;
inline constexpr auto& fixed_color_frag = ::vkkk::fixed_color_frag;
inline constexpr auto& fixed_color_mesh_frag = ::vkkk::fixed_color_mesh_frag;
} // namespace built_in_shader

} // namespace vkkk
