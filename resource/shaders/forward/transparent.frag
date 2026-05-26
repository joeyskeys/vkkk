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

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

void main() {
    float alpha = clamp(material.diffuse.a, 0.0, 1.0);
    if (alpha <= 0.001) {
        discard;
    }

    vec3 normal = normalize(fragNormal);
    vec3 light_dir = normalize(light.lightPos.xyz - fragPos);
    vec3 view_dir = normalize(light.viewPos.xyz - fragPos);
    vec3 reflect_dir = reflect(-light_dir, normal);

    float diff = max(dot(normal, light_dir), 0.0);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), max(material.shininess, 1.0));

    vec3 ambient = material.ambient.rgb * light.lightColor.rgb;
    vec3 diffuse = material.diffuse.rgb * diff * light.lightColor.rgb;
    vec3 specular = material.specular.rgb * spec * light.lightColor.rgb;

    outColor = vec4(ambient + diffuse + specular, alpha);
}
