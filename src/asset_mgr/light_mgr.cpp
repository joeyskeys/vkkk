
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
    PipelineLightStorage&& storage)
{
    pipeline_storages[pipeline_name] = std::move(storage);
}

void LightMgr::unregister_pipeline(const std::string& pipeline_name) {
    pipeline_storages.erase(pipeline_name);
}

const PipelineLightStorage* LightMgr::pipeline_storage(const std::string& pipeline_name) const {
    const auto found = pipeline_storages.find(pipeline_name);
    return found == pipeline_storages.end() ? nullptr : &found->second;
}

}
