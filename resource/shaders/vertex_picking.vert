#version 460

layout(binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} camera;

layout(push_constant) uniform VertexPickingParams {
    float point_size;
} params;

layout(location = 0) in vec3 in_position;
layout(location = 0) flat out uint vertex_id;

void main() {
    gl_Position = camera.proj * camera.view * vec4(in_position, 1.0);
    gl_PointSize = params.point_size;
    vertex_id = uint(gl_VertexIndex);
}
