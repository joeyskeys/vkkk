#pragma once

namespace vkkk
{

inline constexpr const char billboard_text_vert[] = R"(
#version 460

layout(binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} camera;

layout(binding = 1) uniform BillboardTextData {
    vec4 position;
    vec4 size;
} billboard;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUv;

layout(location = 0) out vec2 fragUv;

void main() {
    mat3 inverseViewRotation = transpose(mat3(camera.view));
    vec3 right = normalize(inverseViewRotation[0]);
    vec3 up = normalize(inverseViewRotation[1]);
    vec3 forward = normalize(inverseViewRotation[2]);
    vec3 worldPosition = billboard.position.xyz
        + right * inPosition.x * billboard.size.x
        + up * inPosition.y * billboard.size.y
        + forward * inPosition.z * billboard.size.z;
    gl_Position = camera.proj * camera.view * vec4(worldPosition, 1.0);
    fragUv = inUv;
}
)";

inline constexpr const char billboard_text_frag[] = R"(
#version 460

layout(binding = 2) uniform sampler2D textContent;

layout(location = 0) in vec2 fragUv;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(textContent, fragUv);
}
)";

} // namespace vkkk
