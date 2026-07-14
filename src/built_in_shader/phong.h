#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "built_in_shader/common.h"
#include "utils/sizeable.hpp"

namespace vkkk
{

using PhongTransformUBO = CameraUBO;
using PhongLightUBO = PointLightUBO;

struct PhongInstanceAttrs: public Sizeable<PhongInstanceAttrs> {
    glm::mat4 model{1.0f};
    glm::vec4 ambient{0.1f, 0.1f, 0.1f, 1.0f};
    glm::vec4 diffuse{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 specular{1.0f, 1.0f, 1.0f, 1.0f};
    float shininess{32.0f};
    float _pad0{0.0f}; // std140 tail padding for stable upload size.
    float _pad1{0.0f};
    float _pad2{0.0f};
};

// draw_mode.x: 0=wireframe, 1=shaded, 2=shaded_wireframe
struct PhongDrawModeUBO : public Sizeable<PhongDrawModeUBO> {
    glm::ivec4 draw_mode{1, 0, 0, 0};
};

inline constexpr const char phong_vert[] = R"(
#version 460

layout(binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} ubo;

struct PhongInstanceAttr {
    mat4 model;
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    float shininess;
    float _pad0;
    float _pad1;
    float _pad2;
};

layout(std430, binding = 3) readonly buffer PhongInstanceAttrs {
    PhongInstanceAttr attrs[];
} instance_attrs;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) flat out vec4 fragAmbient;
layout(location = 3) flat out vec4 fragDiffuse;
layout(location = 4) flat out vec4 fragSpecular;
layout(location = 5) flat out float fragShininess;

void main() {
    PhongInstanceAttr inst = instance_attrs.attrs[gl_InstanceIndex];
    mat4 model = inst.model;
    vec4 world_pos = model * vec4(inPosition, 1.0);
    fragPos = world_pos.xyz;
    fragNormal = mat3(transpose(inverse(model))) * inNormal;
    fragAmbient = inst.ambient;
    fragDiffuse = inst.diffuse;
    fragSpecular = inst.specular;
    fragShininess = inst.shininess;
    gl_Position = ubo.proj * ubo.view * world_pos;
}
)";

inline constexpr const char phong_frag[] = R"(
#version 450

layout(binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} camera;

layout(binding = 2) uniform PointLightUBO {
    vec4 vec;
    vec4 color;
} light;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) flat in vec4 fragAmbient;
layout(location = 3) flat in vec4 fragDiffuse;
layout(location = 4) flat in vec4 fragSpecular;
layout(location = 5) flat in float fragShininess;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 normal = normalize(fragNormal);
    vec3 light_dir = normalize(light.vec.xyz - fragPos);
    vec3 camera_pos = (inverse(camera.view) * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    vec3 view_dir = normalize(camera_pos - fragPos);
    vec3 reflect_dir = reflect(-light_dir, normal);

    float diff = max(dot(normal, light_dir), 0.0);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), max(fragShininess, 1.0));

    vec3 ambient = fragAmbient.rgb * light.color.rgb;
    vec3 diffuse = fragDiffuse.rgb * diff * light.color.rgb;
    vec3 specular = fragSpecular.rgb * spec * light.color.rgb;

    outColor = vec4(ambient + diffuse + specular, fragDiffuse.a);
}
)";

namespace built_in_shader
{
using PhongInstanceAttrs = ::vkkk::PhongInstanceAttrs;
inline constexpr auto& phong_vert = ::vkkk::phong_vert;
inline constexpr auto& phong_frag = ::vkkk::phong_frag;
} // namespace built_in_shader

} // namespace vkkk
