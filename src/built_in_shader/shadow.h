#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace vkkk
{

// Shadow data for the scene's single main directional light.
struct MainDirectionalShadowData {
    glm::mat4 light_view_proj{1.0f};
    glm::vec4 direction{0.0f, -1.0f, 0.0f, 0.0f};
};

struct ShadowResolveUBO {
    glm::mat4 inv_view_proj{1.0f};
    // x: shadow-map width, y: shadow-map height, z: depth bias, w: reserved
    glm::vec4 shadow_map_size_bias{2048.0f, 2048.0f, 0.0005f, 0.0f};
    // x: pcf radius (texel units), yzw: reserved
    glm::vec4 pcf_radius_reserved{1.0f, 0.0f, 0.0f, 0.0f};
};

struct ShadowResolveMeshVerticesSSBO {
    glm::vec4 positions[3] = {
        glm::vec4(-1.0f, -1.0f, 0.0f, 1.0f),
        glm::vec4(3.0f, -1.0f, 0.0f, 1.0f),
        glm::vec4(-1.0f, 3.0f, 0.0f, 1.0f)
    };
};

struct ShadowResolveMeshIndicesSSBO {
    glm::uvec4 triangles[1] = {
        glm::uvec4(0u, 1u, 2u, 0u)
    };
};

// Depth-only shadow-map generation for the main directional light (instanced).
inline constexpr const char shadow_map_vert[] = R"(
#version 460

struct MainDirectionalShadowData {
    mat4 lightViewProj;
    vec4 direction;
};

layout(std430, binding = 0) readonly buffer MainDirectionalShadow {
    MainDirectionalShadowData light;
} main_directional_shadow;

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

void main() {
    mat4 model = instance_attrs.attrs[gl_InstanceIndex].model;
    gl_Position = main_directional_shadow.light.lightViewProj * model * vec4(inPosition, 1.0);
}
)";

inline constexpr const char shadow_map_frag[] = R"(
#version 450

void main() {
}
)";

inline constexpr const char shadow_resolve_vert[] = R"(
#version 450

layout(location = 0) out vec2 uv;

void main() {
    vec2 pos = vec2(
        (gl_VertexIndex == 1) ? 3.0 : -1.0,
        (gl_VertexIndex == 2) ? 3.0 : -1.0
    );
    uv = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
)";

inline constexpr const char shadow_resolve_frag[] = R"(
#version 450

layout(binding = 0) uniform sampler2D sceneDepth;
layout(binding = 1) uniform sampler2DShadow shadowMap;

layout(binding = 2) uniform ShadowResolve {
    mat4 invViewProj;
    vec4 shadowMapSizeBias;
    vec4 pcfRadiusReserved;
} shadow_resolve;

struct MainDirectionalShadowData {
    mat4 lightViewProj;
    vec4 direction;
};

layout(std430, binding = 3) readonly buffer MainDirectionalShadow {
    MainDirectionalShadowData light;
} main_directional_shadow;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

vec3 reconstruct_world_pos(vec2 frag_uv, float depth) {
    vec4 ndc = vec4(frag_uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 world = shadow_resolve.invViewProj * ndc;
    return world.xyz / max(world.w, 1e-6);
}

float sample_shadow_pcf(vec3 world_pos) {
    MainDirectionalShadowData light = main_directional_shadow.light;
    vec4 light_clip = light.lightViewProj * vec4(world_pos, 1.0);
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
    float depth = texture(sceneDepth, uv).r;
    if (depth >= 1.0) {
        outColor = vec4(1.0);
        return;
    }

    vec3 world_pos = reconstruct_world_pos(uv, depth);
    float visibility = sample_shadow_pcf(world_pos);
    outColor = vec4(vec3(visibility), 1.0);
}
)";

} // namespace vkkk
