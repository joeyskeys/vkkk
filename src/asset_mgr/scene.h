#pragma once

namespace vkkk
{

class MeshMgr;
class LightMgr;

class Scene {
public:
    Scene();

    void load_scene();

    MeshMgr*    mesh_mgr;
    LightMgr*   light_mgr;
};

}