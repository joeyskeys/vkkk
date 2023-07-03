#version 450

layout (location = 0) in vec3 inPos;

layout (binding = 0) uniform UBO {
    mat4 proj;
    mat4 model;
} ubo;

layout (location = 0) out vec3 outUVW;

void main() {
    outUVW = inPos;
    outUVW.xy *= -1.0;
    gl_Position = ubo.proj * ubo.model * vec4(inPos, 1.0);
}