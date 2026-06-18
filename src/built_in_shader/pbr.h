#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace vkkk
{

struct PBRTransformUBO {
    glm::mat4 model{1.0f};
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};
};

struct PBRMaterialUBO {
    glm::vec4 albedo{1.0f, 1.0f, 1.0f, 1.0f};
    float metallic{0.0f};
    float roughness{0.5f};
    float ao{1.0f};
    float _pad0{0.0f}; // std140 tail padding for stable upload size.
};

struct PBRLightUBO {
    glm::vec4 lightPos{0.0f, 5.0f, 5.0f, 1.0f};
    glm::vec4 lightColor{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 viewPos{0.0f, 0.0f, 5.0f, 1.0f};
};

inline constexpr const char pbr_vert[] = R"(
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
    fragNormal = normalize(mat3(transpose(inverse(ubo.model))) * inNormal);
    gl_Position = ubo.proj * ubo.view * world_pos;
}
)";

inline constexpr const char pbr_frag[] = R"(
#version 450

const float PI = 3.14159265359;

layout(binding = 1) uniform PBRMaterial {
    vec4 albedo;
    float metallic;
    float roughness;
    float ao;
} material;

layout(binding = 2) uniform PBRLight {
    vec4 lightPos;
    vec4 lightColor;
    vec4 viewPos;
} light;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

float distribution_ggx(vec3 n, vec3 h, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float n_dot_h = max(dot(n, h), 0.0);
    float n_dot_h2 = n_dot_h * n_dot_h;
    float num = a2;
    float denom = (n_dot_h2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return num / max(denom, 1e-4);
}

float geometry_schlick_ggx(float n_dot_v, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    float num = n_dot_v;
    float denom = n_dot_v * (1.0 - k) + k;
    return num / max(denom, 1e-4);
}

float geometry_smith(vec3 n, vec3 v, vec3 l, float roughness) {
    float n_dot_v = max(dot(n, v), 0.0);
    float n_dot_l = max(dot(n, l), 0.0);
    float ggx1 = geometry_schlick_ggx(n_dot_v, roughness);
    float ggx2 = geometry_schlick_ggx(n_dot_l, roughness);
    return ggx1 * ggx2;
}

vec3 fresnel_schlick(float cos_theta, vec3 f0) {
    return f0 + (1.0 - f0) * pow(1.0 - cos_theta, 5.0);
}

void main() {
    vec3 n = normalize(fragNormal);
    vec3 v = normalize(light.viewPos.xyz - fragPos);
    vec3 l = normalize(light.lightPos.xyz - fragPos);
    vec3 h = normalize(v + l);

    float distance = length(light.lightPos.xyz - fragPos);
    float attenuation = 1.0 / max(distance * distance, 1e-4);
    vec3 radiance = light.lightColor.rgb * attenuation;

    vec3 albedo = material.albedo.rgb;
    float metallic = clamp(material.metallic, 0.0, 1.0);
    float roughness = clamp(material.roughness, 0.04, 1.0);
    float ao = clamp(material.ao, 0.0, 1.0);

    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    vec3 f = fresnel_schlick(max(dot(h, v), 0.0), f0);
    float ndf = distribution_ggx(n, h, roughness);
    float g = geometry_smith(n, v, l, roughness);

    vec3 numerator = ndf * g * f;
    float denominator = 4.0 * max(dot(n, v), 0.0) * max(dot(n, l), 0.0);
    vec3 specular = numerator / max(denominator, 1e-4);

    vec3 ks = f;
    vec3 kd = (vec3(1.0) - ks) * (1.0 - metallic);
    float n_dot_l = max(dot(n, l), 0.0);

    vec3 lo = (kd * albedo / PI + specular) * radiance * n_dot_l;
    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color = ambient + lo;

    // Simple tone mapping and gamma correction.
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, material.albedo.a);
}
)";

} // namespace vkkk
