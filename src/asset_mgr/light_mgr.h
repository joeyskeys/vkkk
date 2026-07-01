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
    friend class Singleton<LightMgr>;

public:
    void register_pipeline(const std::string& pipeline_name,
        PipelineLightStorage&& storage);
    void unregister_pipeline(const std::string& pipeline_name);

    const PipelineLightStorage* pipeline_storage(const std::string& pipeline_name) const;

private:
    std::unordered_map<std::string, PipelineLightStorage> pipeline_storages;
};

}
