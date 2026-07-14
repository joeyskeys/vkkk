#pragma once

#include <cstdint>

#include <glm/vec4.hpp>

#include "built_in_shader/common.h"
#include "utils/sizeable.hpp"

namespace vkkk
{

// One invocation is dispatched for every (x, y, z) cluster. `max_lights_per_cluster`
// fixes each cluster's range in the flat light-index output buffer.
struct LightClusterParamsUBO : public Sizeable<LightClusterParamsUBO> {
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};
    glm::uvec4 cluster_dims_and_light_count{1u, 1u, 1u, 0u};
    glm::uvec4 config{64u, 1u, 1u, 0u};
    glm::vec4 depth_range{0.1f, 100.0f, 0.0f, 0.0f};
};

// `offset` indexes ClusterLightIndices and `count` is capped by
// LightClusterParamsUBO::config.x.
struct LightClusterEntry : public Sizeable<LightClusterEntry> {
    uint32_t offset{0};
    uint32_t count{0};
    uint32_t _pad0{0};
    uint32_t _pad1{0};
};

// Upload PipelineLightStorage::pt_lights as this SSBO. PipelineLightStorage itself
// cannot be uploaded directly because it contains CPU std::vector instances.
inline constexpr char light_clusterize_comp[] = R"(
#version 460

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(std140, binding = 0) uniform LightClusterParams {
    mat4 view;
    mat4 proj;
    uvec4 cluster_dims_and_light_count; // xyz = cluster dimensions, w = point-light count
    uvec4 config;                       // x = max lights/cluster, y = viewport width, z = viewport height
    vec4 depth_range;                   // x = near plane, y = far plane
} params;

struct PointLight {
    vec4 vec;   // xyz = world-space position
    vec4 color;
    float radius;
    float _pad0;
    float _pad1;
    float _pad2;
};

layout(std430, binding = 1) readonly buffer PointLights {
    PointLight point_lights[];
};

// One uvec4 per cluster: x = offset into ClusterLightIndices, y = count.
layout(std430, binding = 2) writeonly buffer ClusterGrid {
    uvec4 clusters[];
};

layout(std430, binding = 3) writeonly buffer ClusterLightIndices {
    uint light_indices[];
};

float slice_depth(uint slice, uint slice_count) {
    const float near_plane = max(params.depth_range.x, 0.0001);
    const float far_plane = max(params.depth_range.y, near_plane + 0.0001);
    return near_plane * pow(far_plane / near_plane, float(slice) / float(slice_count));
}

bool overlaps_cluster(PointLight light, uvec3 cluster_coord) {
    const vec3 view_pos = (params.view * vec4(light.vec.xyz, 1.0)).xyz;
    const float radius = max(light.radius, 0.0);
    const float light_depth = -view_pos.z;

    const uint z_slices = params.cluster_dims_and_light_count.z;
    const float cluster_near = slice_depth(cluster_coord.z, z_slices);
    const float cluster_far = slice_depth(cluster_coord.z + 1u, z_slices);
    if (light_depth + radius < cluster_near || light_depth - radius > cluster_far) {
        return false;
    }

    // A sphere behind the camera cannot overlap a visible screen tile.
    if (light_depth + radius <= 0.0) {
        return false;
    }

    const vec4 clip_pos = params.proj * vec4(view_pos, 1.0);
    if (clip_pos.w <= 0.0) {
        return false;
    }

    const vec2 ndc_center = clip_pos.xy / clip_pos.w;
    const float projected_radius_x = abs(params.proj[0][0]) * radius / max(light_depth, 0.0001);
    const float projected_radius_y = abs(params.proj[1][1]) * radius / max(light_depth, 0.0001);

    const vec2 tile_a = vec2(cluster_coord.xy) / vec2(params.cluster_dims_and_light_count.xy) * 2.0 - 1.0;
    const vec2 tile_b = vec2(cluster_coord.xy + uvec2(1u)) / vec2(params.cluster_dims_and_light_count.xy) * 2.0 - 1.0;
    const vec2 tile_min = min(tile_a, tile_b);
    const vec2 tile_max = max(tile_a, tile_b);

    return ndc_center.x + projected_radius_x >= tile_min.x
        && ndc_center.x - projected_radius_x <= tile_max.x
        && ndc_center.y + projected_radius_y >= tile_min.y
        && ndc_center.y - projected_radius_y <= tile_max.y;
}

void main() {
    const uvec3 cluster_coord = gl_WorkGroupID.xyz;
    const uvec3 cluster_dims = params.cluster_dims_and_light_count.xyz;
    if (any(greaterThanEqual(cluster_coord, cluster_dims))) {
        return;
    }

    const uint cluster_index = cluster_coord.x
        + cluster_dims.x * (cluster_coord.y + cluster_dims.y * cluster_coord.z);
    const uint max_lights_per_cluster = params.config.x;
    const uint output_offset = cluster_index * max_lights_per_cluster;
    uint light_count = 0u;

    for (uint light_index = 0u;
         light_index < params.cluster_dims_and_light_count.w && light_count < max_lights_per_cluster;
         ++light_index)
    {
        if (overlaps_cluster(point_lights[light_index], cluster_coord)) {
            light_indices[output_offset + light_count] = light_index;
            ++light_count;
        }
    }

    clusters[cluster_index] = uvec4(output_offset, light_count, 0u, 0u);
}
)";

} // namespace vkkk
