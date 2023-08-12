#pragma once

#include "utils/macros.h"

#ifndef GL_core_profile
#include <cstring>
#include <glm/glm.hpp>
using namespace glm;

namespace vkkk
{
#endif

struct PointLight {
    vec4   pos;
    vec4   color;
};

struct DirectionalLight {
    vec4   direction;
    vec4   color;
};

struct SpotLight {
    vec4   pos;
    vec4   direction;
    vec3   color;
    float  angle;
};

#ifndef GL_core_profile 
inline void update_uniform(void* data, void* light_obj, uint32_t n) {
    memcpy(data, light_obj, n);
}

}
#endif