#pragma once

namespace vkkk
{

class DrawableMgr;
class LightMgr;
class PipelineMgr;

class Scene {
public:
    Scene();

    DrawableMgr*    drawable_mgr;
    LightMgr*       light_mgr;
    PipelineMgr*    ppl_mgr;
};

}