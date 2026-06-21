#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace vkkk
{

struct PhongTransformUBO {
    glm::mat4 model{1.0f};
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};
};

struct PhongMaterialUBO {
    glm::vec4 ambient{0.1f, 0.1f, 0.1f, 1.0f};
    glm::vec4 diffuse{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 specular{1.0f, 1.0f, 1.0f, 1.0f};
    float shininess{32.0f};
    float _pad0{0.0f}; // std140 tail padding for stable upload size.
    float _pad1{0.0f};
    float _pad2{0.0f};
};

struct PhongLightUBO {
    glm::vec4 lightPos{0.0f, 5.0f, 5.0f, 1.0f};
    glm::vec4 lightColor{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 viewPos{0.0f, 0.0f, 5.0f, 1.0f};
};

// draw_mode.x: 0=wireframe, 1=shaded, 2=shaded_wireframe
struct PhongDrawModeUBO {
    glm::ivec4 draw_mode{1, 0, 0, 0};
};

struct PhongMeshVerticesSSBO {
    glm::vec4 positions[3] = {
        glm::vec4(-0.5f, -0.5f, 0.0f, 1.0f),
        glm::vec4(0.5f, -0.5f, 0.0f, 1.0f),
        glm::vec4(0.0f, 0.5f, 0.0f, 1.0f)
    };
};

struct PhongMeshNormalsSSBO {
    glm::vec4 normals[3] = {
        glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
        glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
        glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)
    };
};

struct PhongMeshIndicesSSBO {
    glm::uvec4 triangles[1] = {
        glm::uvec4(0u, 1u, 2u, 0u)
    };
};

inline constexpr const char phong_vert[] = R"(
#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;

void main() {
    vec4 world_pos = ubo.model * vec4(inPosition, 1.0);
    fragPos = world_pos.xyz;
    fragNormal = mat3(transpose(inverse(ubo.model))) * inNormal;
    gl_Position = ubo.proj * ubo.view * world_pos;
}
)";

inline constexpr const char phong_vert_instanced[] = R"(
#version 460

layout(std430, binding = 0) readonly buffer InstanceAttrs {
    mat4 model;
    vec4 color;
} instance_attrs;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;

void main() {
    vec4 world_pos = instance_attrs.model * vec4(inPosition, 1.0);
    fragPos = world_pos.xyz;
    fragNormal = mat3(transpose(inverse(instance_attrs.model))) * inNormal;
    gl_Position = ubo.proj * ubo.view * world_pos;
}
)";

inline constexpr const char phong_mesh[] = R"(
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

layout(std430, binding = 3) readonly buffer MeshPositions {
    vec4 positions[3];
} mesh_positions;

layout(std430, binding = 4) readonly buffer MeshNormals {
    vec4 normals[3];
} mesh_normals;

layout(std430, binding = 5) readonly buffer MeshIndices {
    uvec4 triangles[1];
} mesh_indices;

layout(location = 0) out vec3 fragPos[];
layout(location = 1) out vec3 fragNormal[];
layout(location = 2) out vec3 bary[];

layout(binding = 6) uniform DrawMode {
    ivec4 drawMode;
} draw_mode;

void main() {
    const uint vertex_count = 3u;
    const uint triangle_count = 1u;
    const mat3 normal_mat = mat3(transpose(inverse(ubo.model)));
    const vec3 barycentric[3] = vec3[](
        vec3(1.0, 0.0, 0.0),
        vec3(0.0, 1.0, 0.0),
        vec3(0.0, 0.0, 1.0)
    );

    SetMeshOutputsEXT(vertex_count, triangle_count);
    for (uint i = 0; i < vertex_count; ++i) {
        vec4 world_pos = ubo.model * mesh_positions.positions[i];
        gl_MeshVerticesEXT[i].gl_Position = ubo.proj * ubo.view * world_pos;
        fragPos[i] = world_pos.xyz;
        fragNormal[i] = normal_mat * mesh_normals.normals[i].xyz;
        bary[i] = (draw_mode.drawMode.x == 1) ? vec3(0.0) : barycentric[i];
    }
    for (uint i = 0; i < triangle_count; ++i) {
        gl_PrimitiveTriangleIndicesEXT[i] = mesh_indices.triangles[i].xyz;
    }
}
)";

inline constexpr const char phong_frag[] = R"(
#version 450

layout(binding = 1) uniform PhongMaterial {
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    float shininess;
} material;

layout(binding = 2) uniform PhongLight {
    vec4 lightPos;
    vec4 lightColor;
    vec4 viewPos;
} light;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 normal = normalize(fragNormal);
    vec3 light_dir = normalize(light.lightPos.xyz - fragPos);
    vec3 view_dir = normalize(light.viewPos.xyz - fragPos);
    vec3 reflect_dir = reflect(-light_dir, normal);

    float diff = max(dot(normal, light_dir), 0.0);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), max(material.shininess, 1.0));

    vec3 ambient = material.ambient.rgb * light.lightColor.rgb;
    vec3 diffuse = material.diffuse.rgb * diff * light.lightColor.rgb;
    vec3 specular = material.specular.rgb * spec * light.lightColor.rgb;

    outColor = vec4(ambient + diffuse + specular, material.diffuse.a);
}
)";

inline constexpr const char phong_mesh_frag[] = R"(
#version 450

layout(binding = 1) uniform PhongMaterial {
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    float shininess;
} material;

layout(binding = 2) uniform PhongLight {
    vec4 lightPos;
    vec4 lightColor;
    vec4 viewPos;
} light;

layout(binding = 6) uniform DrawMode {
    ivec4 drawMode;
} draw_mode;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 bary;

layout(location = 0) out vec4 outColor;

float wire_factor(vec3 b) {
    vec3 d = fwidth(b);
    vec3 s = smoothstep(vec3(0.0), d * 1.2, b);
    return min(min(s.x, s.y), s.z);
}

vec3 phong_lit_color() {
    vec3 normal = normalize(fragNormal);
    vec3 light_dir = normalize(light.lightPos.xyz - fragPos);
    vec3 view_dir = normalize(light.viewPos.xyz - fragPos);
    vec3 reflect_dir = reflect(-light_dir, normal);

    float diff = max(dot(normal, light_dir), 0.0);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), max(material.shininess, 1.0));

    vec3 ambient = material.ambient.rgb * light.lightColor.rgb;
    vec3 diffuse = material.diffuse.rgb * diff * light.lightColor.rgb;
    vec3 specular = material.specular.rgb * spec * light.lightColor.rgb;
    return ambient + diffuse + specular;
}

void main() {
    const int mode = draw_mode.drawMode.x;
    vec3 shaded = phong_lit_color();
    if (mode == 1) {
        outColor = vec4(shaded, material.diffuse.a);
        return;
    }

    float edge = 1.0 - wire_factor(bary);
    if (mode == 0) {
        if (edge < 0.45) {
            discard;
        }
        outColor = vec4(vec3(0.0), material.diffuse.a);
        return;
    }

    vec3 line = vec3(0.0);
    float wire_alpha = smoothstep(0.25, 0.85, edge);
    outColor = vec4(mix(shaded, line, wire_alpha), material.diffuse.a);
}
)";

} // namespace vkkk
