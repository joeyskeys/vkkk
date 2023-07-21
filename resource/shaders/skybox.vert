#version 450

layout (location = 0) in vec3 inPos;

layout (binding = 0) uniform UBO {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout (location = 0) out vec3 outUVW;

void main() {
    outUVW = inPos;
    outUVW.xy *= -1.0;
    vec4 pos = ubo.proj * ubo.view * vec4(inPos, 1.0);
    //gl_Position = pos.xyww;
    gl_Position = pos;
}