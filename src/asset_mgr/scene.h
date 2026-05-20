#pragma once

namespace vkkk
{

class MeshMgr;
class LightMgr;
class PipelineMgr;

class Scene {
public:
    Scene();

    MeshMgr*        mesh_mgr;
    LightMgr*       light_mgr;
    PipelineMgr*    ppl_mgr;
};

}