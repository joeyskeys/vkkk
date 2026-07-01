#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "built_in_shader/common.h"
#include "utils/singleton.h"
#include "vk_ins/context.hpp"

namespace vkkk
{

struct PipelineLightStorage {
    std::vector<PointLightUBO> pt_lights;
    std::vector<DirectionalLightUBO> dir_lights;
    std::vector<SpotLightUBO> spot_lights;
};

class LightMgr : public Singleton<LightMgr> {
private:
    LightMgr() {}
    LightMgr(const LightMgr&) = delete;
    LightMgr& operator= (const LightMgr&) = delete;

public:
    void register_pipeline(const std::string& pipeline_name,
        PipelineLightStorage&& storage);
    void unregister_pipeline(const std::string& pipeline_name);

    //void update_uniform(const std::string& pipeline_name, uint32_t swapchain_idx, Context* ctx);

    const std::vector<PointLightUBO>& point_lights() const { return pt_lights; }
    const std::vector<DirectionalLightUBO>& directional_lights() const { return dir_lights; }
    const std::vector<SpotLightUBO>& spot_lights() const { return spot_lights; }
    const PipelineLightStorage* pipeline_storage(const std::string& pipeline_name) const;

private:
    std::unordered_map<std::string, PipelineLightStorage> pipeline_storages;
};

}
