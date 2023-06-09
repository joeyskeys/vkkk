#version 450

layout(binding = 1) uniform sampler2D tex_sampler;
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

void main() {
    //outColor = vec4(1.0, 0.0, 0.0, 1.0);
    outColor = texture(tex_sampler, uv);
}