#pragma once

#include "utils/macros.h"

#include <cstring>
#include <glm/glm.hpp>

// Code here cannot be reused in glsl shader file by including.
// Vulkan shader cannot separate struct definition and its declaration.

struct PointLight {
    glm::vec4   pos;
    glm::vec4   color;
    //bool   falloff;
};

struct DirectionalLight {
    glm::vec3   direction;
    glm::vec3   color;
};

struct SpotLight {
    glm::vec3   pos;
    glm::vec3   direction;
    glm::vec3   color;
    float  angle;
};

inline void update_uniform(void* data, void* light_obj, uint32_t n) {
    memcpy(data, light_obj, n);
}