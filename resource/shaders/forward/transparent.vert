#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 bary;

void main() {
    vec4 world_pos = ubo.model * vec4(inPosition, 1.0);
    fragPos = world_pos.xyz;
    fragNormal = mat3(transpose(inverse(ubo.model))) * inNormal;
    // Approximate barycentrics for wireframe visualization in the vertex path.
    int vid = gl_VertexIndex % 3;
    bary = (vid == 0) ? vec3(1.0, 0.0, 0.0)
        : (vid == 1) ? vec3(0.0, 1.0, 0.0)
        : vec3(0.0, 0.0, 1.0);
    gl_Position = ubo.proj * ubo.view * world_pos;
}
