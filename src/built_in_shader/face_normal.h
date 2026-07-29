#pragma once

#include <cstdint>

#include "built_in_shader/common.h"
#include "utils/sizeable.hpp"

namespace vkkk
{

// Shared data for one source mesh. `vertex_stride_floats` is the interleaved
// vertex pitch in floats (6 for VERTEX+NORMAL: pos.xyz then nrm.xyz).
// `use_mesh_normal`: 0 = geometric face normal; non-zero = average of the
// three mesh vertex normals. Both are drawn from the face center.
struct FaceNormalMeshInfoUBO : public Sizeable<FaceNormalMeshInfoUBO> {
    uint32_t vertex_stride_floats{6};
    uint32_t index_count{0};
    uint32_t instance_count{1};
    uint32_t use_mesh_normal{0};
    float normal_length{0.1f};
    float pad0{0.f};
    float pad1{0.f};
    float pad2{0.f};
};

// One SSBO element per instance of the same indexed mesh.
struct FaceNormalParamsUBO : public Sizeable<FaceNormalParamsUBO> {
    glm::mat4 model{1.0f};
    glm::vec4 color{0.2f, 0.9f, 0.3f, 1.0f};
};

// Must match face_normal_task's triangles_per_mesh_workgroup.
// One face → 2 vertices + 1 line; 32 faces fit max_vertices/primitives = 64/32.
inline constexpr uint32_t face_normal_triangles_per_task = 32;

// Draw one task workgroup for every 32 input triangles of every instance:
//   vkCmdDrawMeshTasksEXT(
//       ceil(index_count / 3.0 / 32.0) * instance_count, 1, 1)
//
// Bindings:
//   0 - CameraUBO
//   1 - FaceNormalMeshInfo
//   2 - Vertices  (std430 float[], interleaved; pos = [0..2], nrm = [3..5])
//   3 - Indices   (std430 uint[], indexed triangle list)
//   4 - FaceNormalParams (std430, one model/color pair per instance)
inline constexpr char face_normal_task[] = R"(
#version 460
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 1) in;

struct FaceNormalTaskPayload {
    uint instance_index;
    uint first_triangle;
    uint triangle_count;
};

taskPayloadSharedEXT FaceNormalTaskPayload payload;

layout(std140, binding = 1) uniform FaceNormalMeshInfo {
    uint vertex_stride_floats;
    uint index_count;
    uint instance_count;
    uint use_mesh_normal;
    float normal_length;
    float pad0;
    float pad1;
    float pad2;
} mesh_info;

struct FaceNormalInstance {
    mat4 model;
    vec4 color;
};

layout(std430, binding = 4) readonly buffer FaceNormalParams {
    FaceNormalInstance attrs[];
} instance_attrs;

const uint triangles_per_mesh_workgroup = 32u;

void main() {
    const uint triangle_count = mesh_info.index_count / 3u;
    const uint task_count_per_instance =
        (triangle_count + triangles_per_mesh_workgroup - 1u) / triangles_per_mesh_workgroup;
    const uint instance_index = gl_WorkGroupID.x / max(task_count_per_instance, 1u);
    const uint task_index = gl_WorkGroupID.x % max(task_count_per_instance, 1u);
    const uint first_triangle = task_index * triangles_per_mesh_workgroup;

    if (instance_index >= mesh_info.instance_count || first_triangle >= triangle_count) {
        EmitMeshTasksEXT(0u, 1u, 1u);
        return;
    }

    payload.instance_index = instance_index;
    payload.first_triangle = first_triangle;
    payload.triangle_count = min(triangles_per_mesh_workgroup, triangle_count - first_triangle);
    EmitMeshTasksEXT(1u, 1u, 1u);
}
)";

// Emits one line per input triangle: face center → center + normal * length.
inline constexpr char face_normal_mesh[] = R"(
#version 460
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 32) in;
layout(lines) out;
layout(max_vertices = 64, max_primitives = 32) out;

struct FaceNormalTaskPayload {
    uint instance_index;
    uint first_triangle;
    uint triangle_count;
};

taskPayloadSharedEXT FaceNormalTaskPayload payload;

layout(binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} camera;

layout(std140, binding = 1) uniform FaceNormalMeshInfo {
    uint vertex_stride_floats;
    uint index_count;
    uint instance_count;
    uint use_mesh_normal;
    float normal_length;
    float pad0;
    float pad1;
    float pad2;
} mesh_info;

layout(std430, binding = 2) readonly buffer Vertices {
    float verts[];
};

layout(std430, binding = 3) readonly buffer Indices {
    uint indices[];
};

struct FaceNormalInstance {
    mat4 model;
    vec4 color;
};

layout(std430, binding = 4) readonly buffer FaceNormalParams {
    FaceNormalInstance attrs[];
} instance_attrs;

layout(location = 0) out vec4 out_color[];

vec3 load_position(uint vertex_index) {
    const uint base = vertex_index * mesh_info.vertex_stride_floats;
    return vec3(verts[base + 0u], verts[base + 1u], verts[base + 2u]);
}

vec3 load_normal(uint vertex_index) {
    const uint base = vertex_index * mesh_info.vertex_stride_floats;
    return vec3(verts[base + 3u], verts[base + 4u], verts[base + 5u]);
}

vec3 safe_normalize(vec3 v, vec3 fallback) {
    const float len_sq = dot(v, v);
    if (len_sq > 1e-20) {
        return v * inversesqrt(len_sq);
    }
    return fallback;
}

void main() {
    const uint invocation = gl_LocalInvocationIndex;
    const uint vertex_count = payload.triangle_count * 2u;
    const uint line_count = payload.triangle_count;
    SetMeshOutputsEXT(vertex_count, line_count);

    const FaceNormalInstance instance = instance_attrs.attrs[payload.instance_index];
    const mat3 normal_matrix = mat3(transpose(inverse(instance.model)));

    for (uint triangle = invocation; triangle < payload.triangle_count; triangle += gl_WorkGroupSize.x) {
        const uint input_index = (payload.first_triangle + triangle) * 3u;
        const uint output_vertex = triangle * 2u;

        const uint i0 = indices[input_index + 0u];
        const uint i1 = indices[input_index + 1u];
        const uint i2 = indices[input_index + 2u];

        const vec3 wp0 = (instance.model * vec4(load_position(i0), 1.0)).xyz;
        const vec3 wp1 = (instance.model * vec4(load_position(i1), 1.0)).xyz;
        const vec3 wp2 = (instance.model * vec4(load_position(i2), 1.0)).xyz;
        const vec3 center = (wp0 + wp1 + wp2) * (1.0 / 3.0);

        vec3 normal;
        if (mesh_info.use_mesh_normal != 0u) {
            // Average of mesh vertex normals, transformed to world space.
            const vec3 n_obj = load_normal(i0) + load_normal(i1) + load_normal(i2);
            normal = safe_normalize(normal_matrix * n_obj, vec3(0.0, 1.0, 0.0));
        }
        else {
            // Geometric face normal from world-space positions.
            const vec3 edge0 = wp1 - wp0;
            const vec3 edge1 = wp2 - wp0;
            normal = safe_normalize(cross(edge0, edge1), vec3(0.0, 1.0, 0.0));
        }
        const vec3 tip = center + normal * mesh_info.normal_length;

        gl_MeshVerticesEXT[output_vertex + 0u].gl_Position =
            camera.proj * camera.view * vec4(center, 1.0);
        gl_MeshVerticesEXT[output_vertex + 1u].gl_Position =
            camera.proj * camera.view * vec4(tip, 1.0);
        out_color[output_vertex + 0u] = instance.color;
        out_color[output_vertex + 1u] = instance.color;

        gl_PrimitiveLineIndicesEXT[triangle] = uvec2(output_vertex + 0u, output_vertex + 1u);
    }
}
)";

inline constexpr char face_normal_frag[] = R"(
#version 460

layout(location = 0) in vec4 in_color;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = in_color;
}
)";

} // namespace vkkk
