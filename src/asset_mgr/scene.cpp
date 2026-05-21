
#include "asset_mgr/light_mgr.h"
#include "asset_mgr/drawable_mgr.h"
#include "asset_mgr/scene.h"

namespace vkkk
{

Scene::Scene()
    : drawable_mgr(&DrawableMgr::instance())
    , light_mgr(&LightMgr::instance())
{}

}