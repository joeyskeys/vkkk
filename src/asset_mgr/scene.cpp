
#include "asset_mgr/light_mgr.h"
#include "asset_mgr/drawable_mgr.h"
#include "asset_mgr/scene.h"

namespace vkkk
{

Scene::Scene()
    : drawable_mgr(&DrawableMgr::instance())
    , light_mgr(&LightMgr::instance())
{}

bool Scene::add_object(std::string name, std::string mesh_name, glm::mat4 model) {
    if (name.empty() || mesh_name.empty() || objects.contains(name)) {
        return false;
    }
    objects.emplace(std::move(name), SceneObject{std::move(mesh_name), model});
    return true;
}

bool Scene::remove_object(const std::string& name) {
    return objects.erase(name) != 0;
}

const SceneObject* Scene::find_object(const std::string& name) const {
    const auto found = objects.find(name);
    return found == objects.end() ? nullptr : &found->second;
}

}