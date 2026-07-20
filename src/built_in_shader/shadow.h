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

inline constexpr const char shadow_resolve_mesh[] = R"(
#version 460
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 1) in;
layout(triangles) out;
layout(max_vertices = 3, max_primitives = 1) out;

layout(std430, binding = 4) readonly buffer MeshPositions {
    vec4 positions[3];
} mesh_positions;

layout(std430, binding = 5) readonly buffer MeshIndices {
    uvec4 triangles[1];
} mesh_indices;

layout(location = 0) out vec2 uv[];

void main() {
    const uint vertex_count = 3u;
    const uint triangle_count = 1u;

    SetMeshOutputsEXT(vertex_count, triangle_count);
    for (uint i = 0; i < vertex_count; ++i) {
        vec2 pos = mesh_positions.positions[i].xy;
        gl_MeshVerticesEXT[i].gl_Position = vec4(pos, 0.0, 1.0);
        uv[i] = pos * 0.5 + 0.5;
    }
    for (uint i = 0; i < triangle_count; ++i) {
        gl_PrimitiveTriangleIndicesEXT[i] = mesh_indices.triangles[i].xyz;
    }
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
