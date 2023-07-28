#version 450

layout (location = 0) in vec2 uv;
layout (location = 0) out vec4 out_color;

layout (binding = 0) uniform ivec3 light_cnts;
layout (binding = 1) uniform PointLight point_lights[MAX_POINT_LIGHTS];
layout (binding = 2) uniform DirectionalLight dir_lights[MAX_DIRECTIONAL_LIGHTS];
layout (binding = 3) uniform SpotLight spot_lights[MAX_SPOT_LIGHTS];
layout (binding = 4) uniform sampler2D tex_sampler;

void main() {
    out_color = texture(tex_sampler, uv);
}