#pragma once

#include "built_in_shader/common.h"

namespace vkkk
{

struct MeshInstanceAttr : public Sizeable<MeshInstanceAttr> {
    glm::mat4 model{1.0f};
};

inline constexpr char mesh_utils_task[] = R"(
#version 460
#extension GL_EXT_task_shader : require

layout(local_size_x = 1) in;

struct MeshletBounds {
    vec4 sphere_center_radius; // xyz = center, w = radius
    vec4 cone_axis_cutoff; // xyz = axis, w = cutoff for backface culling
};

layout(binding = 0) uniform CameraUBO {
    mat4 view_proj;
    vec4 frustum_planes[6];
    vec4 camera_pos;
} camera;

layout(binding = 0, std430) readonly buffer BoundsBuffer {
    MeshletBounds meshlet_bounds[];
};

taskNV out TaskData {
    uint meshlet_id;
} OUT;

bool is_frustum_culled(vec3 center, float radius) {
    for (int i = 0; i < 6; ++i) {
        if (dot(camera.frustum_planes[i], vec4(center, 1.0)) < -radius) {
            return true;
        }
    }
    return false;
    }
}

bool is_backface_culled(vec3 center, float radius, vec3 axis, float cutoff) {
    vec3 dir = normalize(center - camera.camera_pos.xyz);
    return dot(dir, axis) >= cutoff + (radius / length(center - camera.camera_pos.xyz));
}

void main() {
    uint m_id = gl_WorkGroupID.x;

    MeshletBounds bounds = meshlet_bounds[m_id];
    vec3 center = bounds.sphere_center_radius.xyz;
    float radius = bounds.sphere_center_radius.w;
    vec3 axis = bounds.cone_axis_cutoff.xyz;
    float cutoff = bounds.cone_axis_cutoff.w;

    if (is_frustum_culled(center, radius) || is_backface_culled(center, radius, axis, cutoff)) {
        EmitMeshTasksEXT(0, 1, 1);
    }
    else {
        OUT.meshlet_id = m_id;
        EmitMeshTasksEXT(1, 1, 1);
    }
}
)";

inline constexpr char mesh_utils_mesh[] = R"(
#version 460
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 32) in;
layout(triangles) out;
layout(max_vertices = 64, max_primitives = 128) out;

taskNV in TaskData {
    uint meshlet_id;
} IN;

struct Meshlet {
    uint vertex_offset;
    uint vertex_count;
    uint primitive_offset;
    uint primitive_count;
};

layout(binding = 0) uniform CameraUBO { mat4 view_proj; } camera;
layout(binding = 2, std430) readonly buffer MeshletBuffer  { Meshlet meshlets[]; };
layout(binding = 3, std430) readonly buffer VertexBuffer   { vec3 positions[]; };
layout(binding = 4, std430) readonly buffer LocalIdxBuffer { uint local_indices[]; }; // Packaged 8-bit or 32-bit indices
layout(binding = 5, std430) readonly buffer MeshletIdxBuffer{ uint meshlet_vertices[]; };

layout(location = 0) out vec3 out_normal; // To fragment shader

void main() {
    uint ti = gl_LocalInvocationIndex;
    uint m_id = IN.meshlet_id; // Read the culled meshlet index

    Meshlet m = meshlets[m_id];

    // 1. Tell the hardware how many primitives/vertices this workgroup emits
    SetMeshOutputsEXT(m.vertex_count, m.primitive_count);

    // 2. Cooperative "Vertex Shader" Pass
    // Loops ensure that if vertex_count > 32, all vertices are still processed
    for (uint i = ti; i < m.vertex_count; i += gl_WorkGroupSize.x) {
        // Find the global vertex resource index
        uint vertex_index = meshlet_vertices[m.vertex_offset + i];
        vec3 pos = positions[vertex_index];

        // Mimic your traditional vertex shader transformation logic here
        gl_MeshVerticesEXT[i].gl_Position = camera.view_proj * vec4(pos, 1.0);
    }

    // 3. Cooperative Topology Pass
    // Reads local indices and maps them straight to the hardware primitive generator
    for (uint i = ti; i < m.primitive_count; i += gl_WorkGroupSize.x) {
        uint pack_idx = m.primitive_offset + (i * 3);
        
        gl_PrimitiveTriangleIndicesEXT[i] = uvec3(
            local_indices[pack_idx + 0],
            local_indices[pack_idx + 1],
            local_indices[pack_idx + 2]
        );
    }
}

)";

// Draw one task workgroup for every 21 input triangles:
// vkCmdDrawMeshTasksEXT(ceil(index_count / 3.0 / 21.0), 1, 1).
// Binding 3 is one packed SSBO containing:
//   uint vertex_count;
//   uint index_count;
//   uint position_words[vertex_count * 3]; // IEEE-754 x, y, z bits
//   uint indices[index_count];             // indexed triangle-list indices
inline constexpr char gen_line_task[] = R"(
#version 460
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 1) in;

struct LineTaskPayload {
    uint first_triangle;
    uint triangle_count;
};

taskPayloadSharedEXT LineTaskPayload payload;

layout(std430, binding = 3) readonly buffer IndexedTriangleListBuffer {
    uint vertex_count;
    uint index_count;
    uint data[];
} mesh;

const uint triangles_per_mesh_workgroup = 21u;

void main() {
    const uint triangle_count = mesh.index_count / 3u;
    const uint first_triangle = gl_WorkGroupID.x * triangles_per_mesh_workgroup;

    if (first_triangle >= triangle_count) {
        EmitMeshTasksEXT(0u, 1u, 1u);
        return;
    }

    payload.first_triangle = first_triangle;
    payload.triangle_count = min(triangles_per_mesh_workgroup, triangle_count - first_triangle);
    EmitMeshTasksEXT(1u, 1u, 1u);
}
)";

// Emits three line primitives per input triangle. Shared triangle edges are emitted
// twice; this preserves the input triangle list exactly and needs no adjacency data.
inline constexpr char gen_line_mesh[] = R"(
#version 460
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 32) in;
layout(lines) out;
layout(max_vertices = 63, max_primitives = 63) out;

struct LineTaskPayload {
    uint first_triangle;
    uint triangle_count;
};

taskPayloadSharedEXT in LineTaskPayload payload;

layout(binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} camera;

layout(std430, binding = 3) readonly buffer IndexedTriangleListBuffer {
    uint vertex_count;
    uint index_count;
    uint data[];
} mesh;

vec3 load_position(uint vertex_index) {
    const uint position_offset = vertex_index * 3u;
    return vec3(
        uintBitsToFloat(mesh.data[position_offset + 0u]),
        uintBitsToFloat(mesh.data[position_offset + 1u]),
        uintBitsToFloat(mesh.data[position_offset + 2u])
    );
}

uint load_index(uint index_index) {
    const uint index_offset = mesh.vertex_count * 3u;
    return mesh.data[index_offset + index_index];
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

        const uint i0 = load_index(input_index + 0u);
        const uint i1 = load_index(input_index + 1u);
        const uint i2 = load_index(input_index + 2u);

        gl_MeshVerticesEXT[output_vertex + 0u].gl_Position =
            camera.proj * camera.view * vec4(load_position(i0), 1.0);
        gl_MeshVerticesEXT[output_vertex + 1u].gl_Position =
            camera.proj * camera.view * vec4(load_position(i1), 1.0);
        gl_MeshVerticesEXT[output_vertex + 2u].gl_Position =
            camera.proj * camera.view * vec4(load_position(i2), 1.0);

        gl_PrimitiveLineIndicesEXT[output_line + 0u] = uvec2(output_vertex + 0u, output_vertex + 1u);
        gl_PrimitiveLineIndicesEXT[output_line + 1u] = uvec2(output_vertex + 1u, output_vertex + 2u);
        gl_PrimitiveLineIndicesEXT[output_line + 2u] = uvec2(output_vertex + 2u, output_vertex + 0u);
    }
}
)";

}