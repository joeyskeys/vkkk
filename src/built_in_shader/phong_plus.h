#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "built_in_shader/common.h"
#include "utils/sizeable.hpp"

namespace vkkk
{

struct PhongPlusInstanceAttrs : public Sizeable<PhongPlusInstanceAttrs> {
    glm::mat4 model{1.0f};
    glm::vec4 ambient{0.1f, 0.1f, 0.1f, 1.0f};
    glm::vec4 diffuse{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 specular{1.0f, 1.0f, 1.0f, 1.0f};
    float shininess{32.0f};
    float _pad0{0.0f};
    float _pad1{0.0f};
    float _pad2{0.0f};
};

inline constexpr const char phong_plus_vert[] = R"(
#version 460

layout(binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} camera;

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
    vec4 world_pos = inst.model * vec4(inPosition, 1.0);
    fragPos = world_pos.xyz;
    fragNormal = mat3(transpose(inverse(inst.model))) * inNormal;
    fragAmbient = inst.ambient;
    fragDiffuse = inst.diffuse;
    fragSpecular = inst.specular;
    fragShininess = inst.shininess;
    gl_Position = camera.proj * camera.view * world_pos;
}
)";

inline constexpr const char phong_plus_frag[] = R"(
#version 460

layout(binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} camera;

layout(std140, binding = 1) uniform LightClusterParams {
    mat4 view;
    mat4 proj;
    uvec4 cluster_dims_and_light_count; // xyz = cluster dimensions
    uvec4 config;                       // x = max lights, y = viewport width, z = viewport height
    vec4 depth_range;                   // x = near plane, y = far plane
} cluster_params;

struct PointLight {
    vec4 vec;   // xyz = world-space position
    vec4 color;
    float radius;
    float _pad0;
    float _pad1;
    float _pad2;
};

layout(std430, binding = 4) readonly buffer PointLights {
    PointLight point_lights[];
};

// x = offset in light_indices; y = number of lights in this cluster.
layout(std430, binding = 5) readonly buffer ClusterGrid {
    uvec4 clusters[];
};

layout(std430, binding = 6) readonly buffer ClusterLightIndices {
    uint light_indices[];
};

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) flat in vec4 fragAmbient;
layout(location = 3) flat in vec4 fragDiffuse;
layout(location = 4) flat in vec4 fragSpecular;
layout(location = 5) flat in float fragShininess;

layout(location = 0) out vec4 outColor;

uint cluster_depth_slice(float view_depth, uint slice_count) {
    float near_plane = max(cluster_params.depth_range.x, 0.0001);
    float far_plane = max(cluster_params.depth_range.y, near_plane + 0.0001);
    float normalized_depth = log(max(view_depth, near_plane) / near_plane)
        / log(far_plane / near_plane);
    return uint(clamp(floor(normalized_depth * float(slice_count)), 0.0, float(slice_count - 1u)));
}

uint fragment_cluster_index() {
    uvec3 cluster_dims = cluster_params.cluster_dims_and_light_count.xyz;
    uint cluster_x = min(
        uint(gl_FragCoord.x * float(cluster_dims.x) / float(cluster_params.config.y)),
        cluster_dims.x - 1u);
    uint cluster_y = min(
        uint(gl_FragCoord.y * float(cluster_dims.y) / float(cluster_params.config.z)),
        cluster_dims.y - 1u);
    float view_depth = -(cluster_params.view * vec4(fragPos, 1.0)).z;
    uint cluster_z = cluster_depth_slice(view_depth, cluster_dims.z);
    return cluster_x + cluster_dims.x * (cluster_y + cluster_dims.y * cluster_z);
}

void main() {
    vec3 normal = normalize(fragNormal);
    vec3 camera_pos = (inverse(camera.view) * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    vec3 view_dir = normalize(camera_pos - fragPos);
    vec3 lit = fragAmbient.rgb;

    uvec4 cluster = clusters[fragment_cluster_index()];
    for (uint i = 0u; i < cluster.y; ++i) {
        PointLight light = point_lights[light_indices[cluster.x + i]];
        vec3 to_light = light.vec.xyz - fragPos;
        float distance_to_light = length(to_light);
        if (distance_to_light >= light.radius) {
            continue;
        }

        vec3 light_dir = to_light / max(distance_to_light, 0.0001);
        vec3 reflect_dir = reflect(-light_dir, normal);
        float diffuse_factor = max(dot(normal, light_dir), 0.0);
        float specular_factor = pow(
            max(dot(view_dir, reflect_dir), 0.0),
            max(fragShininess, 1.0));
        float range_factor = 1.0 - distance_to_light / max(light.radius, 0.0001);
        float attenuation = range_factor * range_factor;

        lit += (fragDiffuse.rgb * diffuse_factor + fragSpecular.rgb * specular_factor)
            * light.color.rgb * attenuation;
    }

    outColor = vec4(lit, fragDiffuse.a);
}
)";

namespace built_in_shader
{
using PhongPlusInstanceAttrs = ::vkkk::PhongPlusInstanceAttrs;
inline constexpr auto& phong_plus_vert = ::vkkk::phong_plus_vert;
inline constexpr auto& phong_plus_frag = ::vkkk::phong_plus_frag;
} // namespace built_in_shader

} // namespace vkkk
