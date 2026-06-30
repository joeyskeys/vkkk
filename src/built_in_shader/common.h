#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "utils/sizeable.hpp"
#include "utils/macros.h"

namespace vkkk
{

enum UBOType {
    UBOType_Camera,
    UBOType_PointLight,
    UBOType_DirectionalLight,
    UBOType_SpotLight
};

enum SSBOType {
    SSBOType_InstanceAttrs
};

struct CameraUBO : public Sizeable<CameraUBO> {
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};
};

struct LightBaseUBO : public Sizeable<LightBaseUBO> {
    glm::vec4 vec{0.f, 0.f, 0.f, 1.f};
    glm::vec4 color{1.f, 1.f, 1.f, 1.f};
};

// here vec member is reused for point light and directional light
// point light: vec is position
// directional light: vec is direction
using PointLightUBO = LightBaseUBO;
using DirectionalLightUBO = LightBaseUBO;

// spot light: vec is position
struct SpotLightUBO : public Sizeable<SpotLightUBO> {
    glm::vec4 dir{1.f, 0.f, 0.f, 0.f};
    float angle;
    float _pad0{0.0f}; // std140 tail padding for stable upload size.
    float _pad1{0.0f};
    float _pad2{0.0f};
};

}