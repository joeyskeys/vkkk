#version 450

layout(binding = 1) uniform Datas {
    vec3 color;
} datas;

layout(location = 0) out vec4 out_color;

void main() {
    out_color = vec4(datas.color, 1);
}