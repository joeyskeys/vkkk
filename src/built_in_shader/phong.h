#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace vkkk
{

struct PhongTransformUBO : public Sizeable<PhongTransformUBO> {
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};
};

struct PhongMaterialUBO : public Sizeable<PhongMaterialUBO> {
    glm::vec4 ambient{0.1f, 0.1f, 0.1f, 1.0f};
    glm::vec4 diffuse{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 specular{1.0f, 1.0f, 1.0f, 1.0f};
    float shininess{32.0f};
    float _pad0{0.0f}; // std140 tail padding for stable upload size.
    float _pad1{0.0f};
    float _pad2{0.0f};
};

struct PhongLightUBO : public Sizeable<PhongLightUBO> {
    glm::vec4 lightPos{0.0f, 5.0f, 5.0f, 1.0f};
    glm::vec4 lightColor{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 viewPos{0.0f, 0.0f, 5.0f, 1.0f};
};

// draw_mode.x: 0=wireframe, 1=shaded, 2=shaded_wireframe
struct PhongDrawModeUBO : public Sizeable<PhongDrawModeUBO> {
    glm::ivec4 draw_mode{1, 0, 0, 0};
};

struct PhongMeshVerticesSSBO : public Sizeable<PhongMeshVerticesSSBO> {
    glm::vec4 positions[3] = {
        glm::vec4(-0.5f, -0.5f, 0.0f, 1.0f),
        glm::vec4(0.5f, -0.5f, 0.0f, 1.0f),
        glm::vec4(0.0f, 0.5f, 0.0f, 1.0f)
    };
};

struct PhongMeshNormalsSSBO : public Sizeable<PhongMeshNormalsSSBO> {
    glm::vec4 normals[3] = {
        glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
        glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
        glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)
    };
};

struct PhongMeshIndicesSSBO : public Sizeable<PhongMeshIndicesSSBO> {
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

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

layout(std430, binding = 3) readonly buffer InstanceAttrs {
    mat4 model[16];
} instance_attrs;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;

void main() {
    mat4 model = instance_attrs.model[gl_InstanceIndex];
    vec4 world_pos = model * vec4(inPosition, 1.0);
    fragPos = world_pos.xyz;
    fragNormal = mat3(transpose(inverse(model))) * inNormal;
    gl_Position = ubo.proj * ubo.view * world_pos;
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

namespace built_in_shader
{
using PhongTransformUBO = ::vkkk::PhongTransformUBO;
using PhongMaterialUBO = ::vkkk::PhongMaterialUBO;
using PhongLightUBO = ::vkkk::PhongLightUBO;
using PhongDrawModeUBO = ::vkkk::PhongDrawModeUBO;
using PhongMeshVerticesSSBO = ::vkkk::PhongMeshVerticesSSBO;
using PhongMeshNormalsSSBO = ::vkkk::PhongMeshNormalsSSBO;
using PhongMeshIndicesSSBO = ::vkkk::PhongMeshIndicesSSBO;
inline constexpr auto& phong_vert = ::vkkk::phong_vert;
inline constexpr auto& phong_vert_instanced = ::vkkk::phong_vert_instanced;
inline constexpr auto& phong_mesh = ::vkkk::phong_mesh;
inline constexpr auto& phong_frag = ::vkkk::phong_frag;
inline constexpr auto& phong_mesh_frag = ::vkkk::phong_mesh_frag;
} // namespace built_in_shader

} // namespace vkkk
