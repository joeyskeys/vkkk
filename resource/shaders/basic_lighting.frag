#version 450

#include "utils/macros.h"

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;
layout (location = 0) out vec4 out_color;

layout (binding = 1) uniform Datas {
    uvec3 light_cnts;
    mat4 normal_xform;
} datas;

layout (binding = 2) uniform PointLight {
    vec4 pos;
    vec4 color;
} point_lights;
//layout (binding = 2) uniform DirectionalLight dir_lights[MAX_DIRECTIONAL_LIGHTS];
//layout (binding = 3) uniform SpotLight spot_lights[MAX_SPOT_LIGHTS];

void main() {
    vec3 world_normal = (datas.normal_xform * vec4(normal, 0)).xyz;
    world_normal = normal;

    /*
    for (int i = 0; i < datas.light_cnts.x; ++i) {
        vec3 light_dir = normalize(point_lights[i].pos - pos);
        out_color += vec4(vec3(1) * max(0, dot(light_dir, world_normal)), 1);
    }
    */
    vec3 light_dir = normalize(point_lights.pos.xyz - pos);
    out_color = vec4(point_lights.color.xyz * max(dot(light_dir, normal), 0), 1);
}