#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "utils/sizeable.hpp"
#include "utils/macros.h"

namespace vkkk
{

// Reflected GLSL block/type names used as Context UBO/SSBO keys (Option E).
// Keep these equal to the shader `uniform` / `buffer` block type names.
namespace buf {
inline constexpr const char* CameraUBO = "CameraUBO";
inline constexpr const char* PointLightUBO = "PointLightUBO";
inline constexpr const char* DirectionalLightUBO = "DirectionalLightUBO";
inline constexpr const char* SpotLightUBO = "SpotLightUBO";
inline constexpr const char* LightClusterParams = "LightClusterParams";
inline constexpr const char* LineGenMeshInfo = "LineGenMeshInfo";
inline constexpr const char* BillboardTextData = "BillboardTextData";
inline constexpr const char* FaceNormalMeshInfo = "FaceNormalMeshInfo";
inline constexpr const char* ShadowResolve = "ShadowResolve";
inline constexpr const char* MainDirectionalShadow = "MainDirectionalShadow";

inline constexpr const char* PhongInstanceAttrs = "PhongInstanceAttrs";
inline constexpr const char* FixedColorInstanceAttrs = "FixedColorInstanceAttrs";
inline constexpr const char* PointLights = "PointLights";
inline constexpr const char* ClusterGrid = "ClusterGrid";
inline constexpr const char* ClusterLightIndices = "ClusterLightIndices";
inline constexpr const char* Vertices = "Vertices";
inline constexpr const char* Indices = "Indices";
inline constexpr const char* LineGenParams = "LineGenParams";
inline constexpr const char* FaceNormalParams = "FaceNormalParams";
}

struct CameraUBO : public Sizeable<CameraUBO> {
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};
};

struct LightBaseUBO : public Sizeable<LightBaseUBO> {
    glm::vec4 vec{0.f, 0.f, 0.f, 1.f};
    glm::vec4 color{1.f, 1.f, 1.f, 1.f};
};

// Point-light position is stored in vec.xyz. Radius is separate from vec.w so
// the base light layout remains suitable for directional lights.
struct PointLightUBO : public LightBaseUBO {
    float radius{1.0f};
    float _pad0{0.0f}; // std140 tail padding for stable upload size.
    float _pad1{0.0f};
    float _pad2{0.0f};
};

// Directional-light vec is its direction.
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