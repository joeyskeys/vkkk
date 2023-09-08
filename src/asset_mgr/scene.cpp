
#include "asset_mgr/light_mgr.h"
#include "asset_mgr/mesh_mgr.h"
#include "asset_mgr/scene.h"
#include "vk_ins/pipeline_mgr.h"

namespace vkkk
{

Scene::Scene()
    : mesh_mgr(&MeshMgrDeprecated::instance())
    , light_mgr(&LightMgr::instance())
    //, ppl_mgr(&PipelineMgr::instance())
{}

}