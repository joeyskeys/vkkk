#pragma once

#include <string>
#include <unordered_map>
#include <utility>

#include <glm/mat4x4.hpp>

#include "vk_ins/context.hpp"

namespace vkkk
{

class DrawableMgr;
class LightMgr;
class Camera;

struct SceneObject {
    std::string mesh_name;
    glm::mat4 model{1.0f};
};

class Scene {
public:
    Scene();

    bool add_object(std::string name, std::string mesh_name, glm::mat4 model = glm::mat4{1.0f});
    bool remove_object(const std::string& name);
    const SceneObject* find_object(const std::string& name) const;

    DrawableMgr*    drawable_mgr;
    LightMgr*       light_mgr;
    Camera*         camera;

private:
    std::unordered_map<std::string, SceneObject> objects;
};

}