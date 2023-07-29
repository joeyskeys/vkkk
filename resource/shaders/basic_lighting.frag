#version 450

#include "utils/macros.h"

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;
layout (location = 0) out vec4 out_color;

layout (binding = 0) uniform Datas {
    uvec3 light_cnts;
    vec3 color;
} datas;

layout (binding = 1) uniform PointLight {
    vec3 pos;
    vec3 color;
} point_lights[MAX_POINT_LIGHTS];
//layout (binding = 2) uniform DirectionalLight dir_lights[MAX_DIRECTIONAL_LIGHTS];
//layout (binding = 3) uniform SpotLight spot_lights[MAX_SPOT_LIGHTS];

void main() {
    out_color = vec4(0);

    for (int i = 0; i < datas.light_cnts.x; ++i) {
        vec3 light_dir = normalize(point_lights[i].pos - pos);
        out_color += vec4(point_lights[i].color * dot(light_dir, normal), 1);
    }
}