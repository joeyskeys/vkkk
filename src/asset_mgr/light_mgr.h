#pragma once

#include <array>
#include <vector>

#include "concepts/lights.h"
#include "utils/singleton.h"

namespace vkkk
{

// Add this for easier python binding
struct LightInfo {
    std::array<PointLight, MAX_POINT_LIGHTS> pt_lights;
    std::array<DirectionalLight, MAX_DIRECTIONAL_LIGHTS> dir_lights;
    std::array<SpotLight, MAX_SPOT_LIGHTS> spot_lights;
};

class LightMgr : public Singleton<LightMgr> {
private:
    LightMgr() {}
    friend class Singleton<LightMgr>;
    LightMgr(const LightMgr&) = delete;
    LightMgr& operator= (const LightMgr&) = delete;
    friend class Singleton<LightMgr>;

public:
    inline void add_pt_light(const glm::vec4& pos, const glm::vec4& color) {
        pt_lights.emplace_back(pos, color);
    }

    inline void add_dir_light(const glm::vec4& dir, const glm::vec4& color) {
        dir_lights.emplace_back(dir, color);
    }

    inline void add_spot_light(const glm::vec4& pos,
        const glm::vec4& dir, const glm::vec3& color, const float angle)
    {
        spot_lights.emplace_back(pos, dir, color, angle);
    }

    void update_uniform(void* data) const;
    void update_uniform(LightInfo& infos) const;

private:
    // since the lights.h header is shared for shader code, we cannot
    // have a traditions OOP design, and hence here in the manager,
    // we use a kinda dumb way to manage them
    std::vector<PointLight> pt_lights;
    std::vector<DirectionalLight> dir_lights;
    std::vector<SpotLight> spot_lights;
};

}