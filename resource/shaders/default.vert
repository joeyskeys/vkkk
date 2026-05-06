#version 450

layout(location = 0) out vec3 fragColor;

layout(binding = 0) uniform Camera {
    mat4 view;
    mat4 proj;
} camera;

vec2 positions[3] = vec2[](
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

void main() {
    vec4 model_pos = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    gl_Position = camera.proj * camera.view * model_pos;
    fragColor = colors[gl_VertexIndex];
}
