#pragma once

#include <cstdint>

#include "built_in_shader/common.h"
#include "utils/sizeable.hpp"

namespace vkkk
{

// Shared data for one source mesh. `vertex_stride_floats` is the interleaved
// vertex pitch in floats (6 for VERTEX+NORMAL as in renderer_main_fp).
struct LineGenMeshInfoUBO : public Sizeable<LineGenMeshInfoUBO> {
    uint32_t vertex_stride_floats{6};
    uint32_t index_count{0};
    uint32_t instance_count{1};
    uint32_t _pad0{0};
};

// One SSBO element per instance of the same indexed mesh.
struct LineGenParamsUBO : public Sizeable<LineGenParamsUBO> {
    glm::mat4 model{1.0f};
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
};

// Must match gen_line_task's triangles_per_mesh_workgroup.
inline constexpr uint32_t line_gen_triangles_per_task = 21;

// Draw one task workgroup for every 21 input triangles of every instance:
//   vkCmdDrawMeshTasksEXT(
//       ceil(index_count / 3.0 / 21.0) * instance_count, 1, 1)
//
// Bindings:
//   0 - CameraUBO
//   1 - LineGenMeshInfo
//   2 - Vertices  (std430 float[], interleaved; position = first 3 floats)
//   3 - Indices   (std430 uint[], indexed triangle list)
//   4 - LineGenParams (std430, one model/color pair per instance)
//
// 21 is max_vertices/3 and max_primitives/3 with the mesh shader limits below
// (3 output verts + 3 line prims per input triangle).
inline constexpr char gen_line_task[] = R"(
#version 460
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 1) in;

struct LineTaskPayload {
    uint instance_index;
    uint first_triangle;
    uint triangle_count;
};

taskPayloadSharedEXT LineTaskPayload payload;

layout(std140, binding = 1) uniform LineGenMeshInfo {
    uint vertex_stride_floats;
    uint index_count;
    uint instance_count;
    uint _pad0;
} mesh_info;

struct LineGenInstance {
    mat4 model;
    vec4 color;
};

layout(std430, binding = 4) readonly buffer LineGenParams {
    LineGenInstance attrs[];
} instance_attrs;

const uint triangles_per_mesh_workgroup = 21u;

void main() {
    const uint triangle_count = mesh_info.index_count / 3u;
    const uint task_count_per_instance =
        (triangle_count + triangles_per_mesh_workgroup - 1u) / triangles_per_mesh_workgroup;
    const uint instance_index = gl_WorkGroupID.x / task_count_per_instance;
    const uint task_index = gl_WorkGroupID.x % task_count_per_instance;
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

// Emits three line primitives per input triangle. Shared edges are emitted
// twice; this matches the source triangle list with no adjacency data.
inline constexpr char gen_line_mesh[] = R"(
#version 460
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 32) in;
layout(lines) out;
layout(max_vertices = 63, max_primitives = 63) out;

struct LineTaskPayload {
    uint instance_index;
    uint first_triangle;
    uint triangle_count;
};

taskPayloadSharedEXT LineTaskPayload payload;

layout(binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} camera;

layout(std140, binding = 1) uniform LineGenMeshInfo {
    uint vertex_stride_floats;
    uint index_count;
    uint instance_count;
    uint _pad0;
} mesh_info;

layout(std430, binding = 2) readonly buffer Vertices {
    float verts[];
};

layout(std430, binding = 3) readonly buffer Indices {
    uint indices[];
};

struct LineGenInstance {
    mat4 model;
    vec4 color;
};

layout(std430, binding = 4) readonly buffer LineGenParams {
    LineGenInstance attrs[];
} instance_attrs;

layout(location = 0) out vec4 out_color[];

vec3 load_position(uint vertex_index) {
    const uint base = vertex_index * mesh_info.vertex_stride_floats;
    return vec3(verts[base + 0u], verts[base + 1u], verts[base + 2u]);
}

void main() {
    const uint invocation = gl_LocalInvocationIndex;
    const uint vertex_count = payload.triangle_count * 3u;
    const uint line_count = payload.triangle_count * 3u;
    SetMeshOutputsEXT(vertex_count, line_count);

    for (uint triangle = invocation; triangle < payload.triangle_count; triangle += gl_WorkGroupSize.x) {
        const uint input_index = (payload.first_triangle + triangle) * 3u;
        const uint output_vertex = triangle * 3u;
        const uint output_line = triangle * 3u;

        const uint i0 = indices[input_index + 0u];
        const uint i1 = indices[input_index + 1u];
        const uint i2 = indices[input_index + 2u];
        const LineGenInstance instance = instance_attrs.attrs[payload.instance_index];

        gl_MeshVerticesEXT[output_vertex + 0u].gl_Position =
            camera.proj * camera.view * instance.model * vec4(load_position(i0), 1.0);
        gl_MeshVerticesEXT[output_vertex + 1u].gl_Position =
            camera.proj * camera.view * instance.model * vec4(load_position(i1), 1.0);
        gl_MeshVerticesEXT[output_vertex + 2u].gl_Position =
            camera.proj * camera.view * instance.model * vec4(load_position(i2), 1.0);
        out_color[output_vertex + 0u] = instance.color;
        out_color[output_vertex + 1u] = instance.color;
        out_color[output_vertex + 2u] = instance.color;

        gl_PrimitiveLineIndicesEXT[output_line + 0u] = uvec2(output_vertex + 0u, output_vertex + 1u);
        gl_PrimitiveLineIndicesEXT[output_line + 1u] = uvec2(output_vertex + 1u, output_vertex + 2u);
        gl_PrimitiveLineIndicesEXT[output_line + 2u] = uvec2(output_vertex + 2u, output_vertex + 0u);
    }
}
)";

// Required for color-target pipelines: mesh shaders only emit geometry.
inline constexpr char gen_line_frag[] = R"(
#version 460

layout(location = 0) in vec4 in_color;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = in_color;
}
)";

} // namespace vkkk
