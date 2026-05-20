#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace vkkk::built_in_shader
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

} // namespace vkkk::built_in_shader
