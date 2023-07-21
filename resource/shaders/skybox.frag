#version 450

layout (binding = 1) uniform samplerCube cubemap_spl;

layout (location = 0) in vec3 inUVW;

layout (location = 0) out vec4 outColor;

void main() {
    //outColor = texture(cubemap_spl, inUVW);
    outColor = vec4(0.5, 0.3, 0.3, 1.0);
}