#version 450

#include "concepts/lights.h"

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;
layout (location = 0) out vec4 out_color;

layout (binding = 1) uniform LightInfo {
    PointLight pt_lights[MAX_POINT_LIGHTS];
} light_info;
//layout (binding = 2) uniform DirectionalLight dir_lights[MAX_DIRECTIONAL_LIGHTS];
//layout (binding = 3) uniform SpotLight spot_lights[MAX_SPOT_LIGHTS];

void main() {
    vec3 frag_color = vec3(0);
    for (int i = 0; i < MAX_POINT_LIGHTS; ++i) {
        vec3 light_dir = normalize(light_info.pt_lights[i].pos.xyz - pos);
        frag_color += clamp(light_info.pt_lights[i].color.xyz *
            max(0, dot(light_dir, normal)), 0, 1);
    }
    //vec3 light_dir = normalize(point_lights.pos.xyz - pos);
    //out_color = vec4(point_lights.color.xyz * max(dot(light_dir, normal), 0), 1);
    out_color = vec4(frag_color, 1);
}