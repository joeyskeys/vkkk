#version 450

layout(binding = 1) uniform PhongMaterial {
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    float shininess;
} material;

layout(binding = 2) uniform PhongLight {
    vec4 lightPos;
    vec4 lightColor;
    vec4 viewPos;
} light;

layout(binding = 3) uniform sampler2DShadow shadowMap;

layout(binding = 4) uniform ShadowParams {
    mat4 lightSpace;
    vec4 params; // x = bias, y = pcf radius (texels), z/w unused
} shadow;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

float shadow_factor(vec4 light_space_pos) {
    vec3 proj = light_space_pos.xyz / light_space_pos.w;
    proj = proj * 0.5 + 0.5;
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0 || proj.z < 0.0 || proj.z > 1.0) {
        return 1.0;
    }

    float bias = shadow.params.x;
    float current_depth = proj.z - bias;
    float shadow_sum = 0.0;
    vec2 texel = 1.0 / textureSize(shadowMap, 0);
    float radius = max(shadow.params.y, 0.5);

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texel * radius;
            vec3 sample_coord = vec3(proj.xy + offset, current_depth);
            shadow_sum += texture(shadowMap, sample_coord);
        }
    }
    return shadow_sum / 9.0;
}

void main() {
    vec3 normal = normalize(fragNormal);
    vec3 light_dir = normalize(light.lightPos.xyz - fragPos);
    vec3 view_dir = normalize(light.viewPos.xyz - fragPos);
    vec3 reflect_dir = reflect(-light_dir, normal);

    float diff = max(dot(normal, light_dir), 0.0);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), max(material.shininess, 1.0));

    vec3 ambient = material.ambient.rgb * light.lightColor.rgb;
    vec3 diffuse = material.diffuse.rgb * diff * light.lightColor.rgb;
    vec3 specular = material.specular.rgb * spec * light.lightColor.rgb;

    vec4 light_space = shadow.lightSpace * vec4(fragPos, 1.0);
    float visibility = shadow_factor(light_space);

    vec3 lit = ambient + (diffuse + specular) * visibility;
    outColor = vec4(lit, material.diffuse.a);
}
