#pragma once

#include "built_in_shader/phong.h"
#include "built_in_shader/shadow.h"

namespace vkkk
{

// Instanced Phong vertex path — same as phong.h.
inline constexpr const char phong_shadow_vert[] = R"(
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

// Instanced Phong fragment path with PCF shadow sampling from the main directional
// shadow map (MainDirectionalShadow + ShadowResolve bias/size, matching shadow.h).
inline constexpr const char phong_shadow_frag[] = R"(
#version 450

layout(binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} camera;

layout(binding = 1) uniform sampler2DShadow shadowMap;

layout(binding = 2) uniform PointLightUBO {
    vec4 vec;
    vec4 color;
} light;

layout(binding = 4) uniform ShadowResolve {
    mat4 invViewProj;
    vec4 shadowMapSizeBias;
    vec4 pcfRadiusReserved;
} shadow_resolve;

layout(binding = 5) uniform MainDirectionalShadow {
    mat4 lightViewProj;
    vec4 direction;
} main_directional_shadow;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) flat in vec4 fragAmbient;
layout(location = 3) flat in vec4 fragDiffuse;
layout(location = 4) flat in vec4 fragSpecular;
layout(location = 5) flat in float fragShininess;

layout(location = 0) out vec4 outColor;

float sample_shadow_pcf(vec3 world_pos) {
    vec4 light_clip = main_directional_shadow.lightViewProj * vec4(world_pos, 1.0);
    vec3 light_ndc = light_clip.xyz / max(light_clip.w, 1e-6);
    if (light_ndc.x < -1.0 || light_ndc.x > 1.0 || light_ndc.y < -1.0 || light_ndc.y > 1.0) {
        return 1.0;
    }
    if (light_ndc.z < 0.0 || light_ndc.z > 1.0) {
        return 1.0;
    }

    vec2 shadow_uv = light_ndc.xy * 0.5 + 0.5;
    float shadow_map_w = max(shadow_resolve.shadowMapSizeBias.x, 1.0);
    float shadow_map_h = max(shadow_resolve.shadowMapSizeBias.y, 1.0);
    vec2 texel = vec2(1.0 / shadow_map_w, 1.0 / shadow_map_h);
    int radius = int(max(shadow_resolve.pcfRadiusReserved.x, 0.0));
    float bias = shadow_resolve.shadowMapSizeBias.z;

    float sum = 0.0;
    int taps = 0;
    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            vec2 tap_uv = shadow_uv + vec2(float(x), float(y)) * texel;
            sum += texture(shadowMap, vec3(tap_uv, light_ndc.z - bias));
            taps += 1;
        }
    }
    return (taps > 0) ? (sum / float(taps)) : 1.0;
}

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

    float visibility = sample_shadow_pcf(fragPos);
    outColor = vec4(ambient + (diffuse + specular) * visibility, fragDiffuse.a);
}
)";

namespace built_in_shader
{
inline constexpr auto& phong_shadow_vert = ::vkkk::phong_shadow_vert;
inline constexpr auto& phong_shadow_frag = ::vkkk::phong_shadow_frag;
} // namespace built_in_shader

} // namespace vkkk
