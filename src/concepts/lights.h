#pragma once

#include "utils/macros.h"

#if HOST_CODE
#include <cstring>
#include <glm/glm.hpp>
using namespace glm;
#endif

struct PointLight {
    vec3   pos;
    vec3   color;
#if HOST_CODE
    bool   falloff;
#endif
};

struct DirectionalLight {
    vec3   direction;
    vec3   color;
};

struct SpotLight {
    vec3   pos;
    vec3   direction;
    vec3   color;
    float  angle;
};

#if HOST_CODE
inline void update_uniform(void* data, void* light_obj, uint32_t n) {
    memcpy(data, light_obj, n);
}
#endif