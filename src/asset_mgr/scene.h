#pragma once

#include <unordered_map>

#include "vk_ins/context.hpp"

namespace vkkk
{

class DrawableMgr;
class LightMgr;
class Camera;

class Scene {
public:
    Scene();

    DrawableMgr*    drawable_mgr;
    LightMgr*       light_mgr;
    Camera*         camera;
};

}