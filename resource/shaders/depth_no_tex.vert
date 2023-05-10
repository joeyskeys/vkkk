
#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 0) out vec3 fragColor;

void main() {
    mat4 view = mat4(
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0, -10.0, 1.0
    );
    mat4 proj = mat4(
        2.3787, 0.0, 0.0, 0.0,
        0.0,    3.1716, 0.0, 0.0,
        0.0, 0.0, -1.02, -1,
        0.0, 0.0, -1.02, 0.0
    );
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    //gl_Position = proj * view * vec4(inPosition, 1.0);
    fragColor = vec3(1.0, 0.2, 0.2);
}