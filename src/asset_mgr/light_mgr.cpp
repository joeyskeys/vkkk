
#include "asset_mgr/light_mgr.h"

#include <algorithm>
#include <cstring>

namespace vkkk
{

namespace
{

template <typename T>
void copy_lights(std::vector<T>& dest, const std::vector<T>& src) {
    if (dest.empty()) {
        return;
    }
    const size_t count = std::min(dest.size(), src.size());
    if (count > 0) {
        std::memcpy(dest.data(), src.data(), count * sizeof(T));
    }
    for (size_t i = count; i < dest.size(); ++i) {
        dest[i] = T{};
    }
}

} // namespace

void LightMgr::register_pipeline(const std::string& pipeline_name,
    const std::unordered_map<UBOType, UBO>& ubos)
{
    PipelineLightStorage storage;
    if (const auto found = ubos.find(UBOType_PointLight); found != ubos.end()) {
        storage.pt_lights.resize(found->second.vecsize);
    }
    if (const auto found = ubos.find(UBOType_DirectionalLight); found != ubos.end()) {
        storage.dir_lights.resize(found->second.vecsize);
    }
    if (const auto found = ubos.find(UBOType_SpotLight); found != ubos.end()) {
        storage.spot_lights.resize(found->second.vecsize);
    }
    pipeline_storages_[pipeline_name] = std::move(storage);
}

void LightMgr::unregister_pipeline(const std::string& pipeline_name) {
    pipeline_storages_.erase(pipeline_name);
}

const PipelineLightStorage* LightMgr::pipeline_storage(const std::string& pipeline_name) const {
    const auto found = pipeline_storages_.find(pipeline_name);
    return found == pipeline_storages_.end() ? nullptr : &found->second;
}

/*
void LightMgr::update_uniform(const std::string& pipeline_name, uint32_t swapchain_idx, Context* ctx) {
    if (!ctx) {
        return;
    }

    const auto storage_it = pipeline_storages_.find(pipeline_name);
    if (storage_it == pipeline_storages_.end()) {
        return;
    }

    const auto pipeline_it = ctx->pipelines.find(pipeline_name);
    if (pipeline_it == ctx->pipelines.end()) {
        return;
    }

    fill_pipeline_storage(storage_it->second);
    const auto& pipeline = pipeline_it->second;
    const auto& storage = storage_it->second;

    const auto sync_ubo = [&](UBOType type, const void* data, size_t byte_size) {
        if (byte_size == 0) {
            return;
        }
        const auto ubo_it = pipeline.ubos.find(type);
        if (ubo_it == pipeline.ubos.end() || swapchain_idx >= ubo_it->second.memos.size()) {
            return;
        }
        ctx->sync_uniform(ubo_it->second.memos[swapchain_idx], data, static_cast<uint32_t>(byte_size));
    };

    sync_ubo(UBOType_PointLight, storage.pt_lights.data(), storage.pt_lights.size() * sizeof(PointLightUBO));
    sync_ubo(UBOType_DirectionalLight, storage.dir_lights.data(),
        storage.dir_lights.size() * sizeof(DirectionalLightUBO));
    sync_ubo(UBOType_SpotLight, storage.spot_lights.data(),
        storage.spot_lights.size() * sizeof(SpotLightUBO));
}
*/

}
