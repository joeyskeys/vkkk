#version 450

#include "concepts/lights.h"

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;
layout (location = 0) out vec4 out_color;

layout (binding = 1) uniform LightInfos {
    PointLight pt_lights[MAX_POINT_LIGHTS];
    DirectionalLight dir_lights[MAX_DIRECTIONAL_LIGHTS];
    SpotLight spot_lights[MAX_SPOT_LIGHTS];
} infos;

void main() {
    vec3 frag_color = vec3(0);
    for (int i = 0; i < MAX_POINT_LIGHTS; ++i) {
        vec3 light_dir = normalize(infos.pt_lights[i].pos.xyz - pos);
        frag_color += clamp(infos.pt_lights[i].color.xyz *
            max(0, dot(light_dir, normal)), 0, 1);
    }
    for (int i = 0; i < MAX_DIRECTIONAL_LIGHTS; ++i) {
        frag_color += clamp(infos.dir_lights[i].color.xyz *
            max(0, dot(-infos.dir_lights[i].direction.xyz, normal)), 0, 1);
    }
    for (int i = 0; i < MAX_SPOT_LIGHTS; ++i) {
        vec3 light_dir = normalize(infos.spot_lights[i].pos.xyz - pos);
        frag_color += clamp(infos.spot_lights[i].color *
            step(0, radians(infos.spot_lights[i].angle) - acos(dot(infos.spot_lights[i].direction.xyz, -light_dir)))
            * max(0, dot(light_dir, normal)), 0, 1);
    }
    out_color = vec4(frag_color, 1);
}