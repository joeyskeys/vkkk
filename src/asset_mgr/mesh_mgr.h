#pragma once

#include <filesystem>

#include "asset_mgr/mesh.h"
#include "utils/singleton.h"

namespace fs=std::filesystem;

namespace vkkk
{

class VkWrappedInstance;

class MeshMgr : public Singleton<MeshMgr> {
public:
    MeshMgr(VkWrappedInstance*);
    void load_file(const fs::path &path, uint32_t flag);
    void add_box(const void *min, const void *max);

private:
    void process_node(aiNode *node, const aiScene *scene, uint32_t flag);

public:
    std::vector<Mesh>   meshes;

private:
    VkWrappedInstance*  ins;
};

}