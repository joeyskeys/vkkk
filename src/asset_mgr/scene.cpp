
#include "asset_mgr/light_mgr.h"
#include "asset_mgr/mesh_mgr.h"
#include "asset_mgr/scene.h"

namespace vkkk
{

Scene::Scene()
    : mesh_mgr(&MeshMgr::instance())
    , light_mgr(&LightMgr::instance())
{}

}