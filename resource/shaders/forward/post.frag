#version 450

layout(binding = 0) uniform PostParams {
    float exposure;
    float gamma;
    float _pad0;
    float _pad1;
} post;

layout(binding = 1) uniform sampler2D sceneColor;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 color = texture(sceneColor, fragUV).rgb;
    color = vec3(1.0) - exp(-color * max(post.exposure, 0.001));
    color = pow(color, vec3(1.0 / max(post.gamma, 0.001)));
    outColor = vec4(color, 1.0);
}
