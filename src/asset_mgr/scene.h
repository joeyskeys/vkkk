#pragma once

namespace vkkk
{

class DrawableMgr;
class LightMgr;

class Scene {
public:
    Scene();

    DrawableMgr*    drawable_mgr;
    LightMgr*       light_mgr;
};

}