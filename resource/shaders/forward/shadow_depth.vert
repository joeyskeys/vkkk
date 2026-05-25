#version 450

layout(binding = 0) uniform ShadowTransform {
    mat4 model;
    mat4 lightView;
    mat4 lightProj;
} shadow;

layout(location = 0) in vec3 inPosition;

void main() {
    gl_Position = shadow.lightProj * shadow.lightView * shadow.model * vec4(inPosition, 1.0);
}
