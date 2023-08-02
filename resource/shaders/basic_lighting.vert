#version 450

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_uv;

layout (location = 0) out vec3 out_pos;
layout (location = 1) out vec3 out_normal;
layout (location = 2) out vec2 out_uv;

layout (binding = 0) uniform Xforms {
    mat4 transform;
} xforms;

void main() {
    gl_Position = xforms.transform * vec4(in_pos, 1.0);
    out_pos = in_pos;
    out_normal = in_normal;
    out_uv = in_uv;
}