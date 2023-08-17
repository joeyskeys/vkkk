
#include "asset_mgr/light_mgr.h"
#include "utils/macros.h"

namespace vkkk
{

void LightMgr::update_uniform(void* data) const {
    /*
    // This code doesn't save much typing and have to occupy more stack
    // space, meaning (a little bit) larger binary...
    // TODO: Probably the compiler will do the loop unfold, leave the code
    // here and come back to look into the assembly later...

    std::array<void*, 3> datas{pt_lights.data(), dir_lights.data(), spot_lights.data()};
    std::array<uint32_t, 3> dyn_sizes{pt_lights.size(), dir_lights.size(), spot_lights.size()};
    std::array<uint32_t, 3> fixed_sizes{MAX_POINT_LIGHTS, MAX_DIRECTIONAL_LIGHTS, MAX_SPOT_LIGHTS};
    constexpr std::array<uint32_t, 3> obj_sizes{sizeof(PointLight), sizeof(DirectionalLight), sizeof(SpotLight)};

    char* dest = data;
    for (int i = 0; i < 3; ++i) {
        uint32_t cnt = dyn_sizes[i] < fixed_sizes[i] ? dyn_sizes[i] : fixed_sizes[i];
        uint32_t size = cnt * obj_sizes[i];
        memcpy(dest, datas[i], size);
        dest += obj_sizes[i] * fixed_sizes[i];
    }
    */

    char* dest = reinterpret_cast<char*>(data);
    uint32_t cnt = pt_lights.size() < MAX_POINT_LIGHTS ? pt_lights.size() : MAX_POINT_LIGHTS;
    if (cnt > 0) {
        uint32_t size = cnt * sizeof(PointLight);
        memcpy(dest, pt_lights.data(), size);
    }

    dest += MAX_POINT_LIGHTS * sizeof(PointLight);
    cnt = dir_lights.size() < MAX_DIRECTIONAL_LIGHTS ? dir_lights.size() : MAX_DIRECTIONAL_LIGHTS;
    if (cnt > 0) {
        uint32_t size = cnt * sizeof(DirectionalLight);
        memcpy(dest, dir_lights.data(), size);
    }

    dest += MAX_DIRECTIONAL_LIGHTS * sizeof(DirectionalLight);
    cnt = spot_lights.size() < MAX_SPOT_LIGHTS ? spot_lights.size() : MAX_SPOT_LIGHTS;
    if (cnt > 0) {
        uint32_t size = cnt * sizeof(SpotLight);
        memcpy(dest, spot_lights.data(), size);
    }
}

void LightMgr::update_uniform(LightInfo& infos) const {
    uint32_t cnt = pt_lights.size() < MAX_POINT_LIGHTS ? pt_lights.size() : MAX_POINT_LIGHTS;
    if (cnt > 0)
        memcpy(&infos.pt_lights, pt_lights.data(), cnt * sizeof(PointLight));

    cnt = dir_lights.size() < MAX_DIRECTIONAL_LIGHTS ? dir_lights.size() : MAX_DIRECTIONAL_LIGHTS;
    if (cnt > 0)
        memcpy(&infos.dir_lights, dir_lights.data(), cnt * sizeof(DirectionalLight));

    cnt = spot_lights.size() < MAX_SPOT_LIGHTS ? spot_lights.size() : MAX_SPOT_LIGHTS;
    if (cnt > 0)
        memcpy(&infos.spot_lights, spot_lights.data(), cnt * sizeof(SpotLight));
}

}