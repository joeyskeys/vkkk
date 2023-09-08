#pragma once

namespace vkkk
{

class MeshMgrDeprecated;
class LightMgr;
class PipelineMgr;

class Scene {
public:
    Scene();

    MeshMgrDeprecated*        mesh_mgr;
    LightMgr*       light_mgr;
    PipelineMgr*    ppl_mgr;
};

}