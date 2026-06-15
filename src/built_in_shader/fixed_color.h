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

struct FixedColorMeshVerticesSSBO {
    glm::vec4 positions[3] = {
        glm::vec4(-0.5f, -0.5f, 0.0f, 1.0f),
        glm::vec4(0.5f, -0.5f, 0.0f, 1.0f),
        glm::vec4(0.0f, 0.5f, 0.0f, 1.0f)
    };
};

struct FixedColorMeshIndicesSSBO {
    glm::uvec4 triangles[1] = {
        glm::uvec4(0u, 1u, 2u, 0u)
    };
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

inline constexpr const char fixed_color_mesh[] = R"(
#version 460
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 1) in;
layout(triangles) out;
layout(max_vertices = 3, max_primitives = 1) out;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(std430, binding = 2) readonly buffer MeshPositions {
    vec4 positions[3];
} mesh_positions;

layout(std430, binding = 3) readonly buffer MeshIndices {
    uvec4 triangles[1];
} mesh_indices;

void main() {
    const uint vertex_count = 3u;
    const uint triangle_count = 1u;

    SetMeshOutputsEXT(vertex_count, triangle_count);
    for (uint i = 0; i < vertex_count; ++i) {
        gl_MeshVerticesEXT[i].gl_Position = ubo.proj * ubo.view * ubo.model * mesh_positions.positions[i];
    }
    for (uint i = 0; i < triangle_count; ++i) {
        gl_PrimitiveTriangleIndicesEXT[i] = mesh_indices.triangles[i].xyz;
    }
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
