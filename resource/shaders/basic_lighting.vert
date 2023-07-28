#version 450

#include "concepts/lights.h"

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec2 in_uv;

layout (location = 0) out vec2 out_uv;

layout (binding = 0) uniform mat4 transform;

void main() {
    gl_Position = transform * vec4(in_pos, 1.0);
    out_uv = uv;
}