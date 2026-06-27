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

}