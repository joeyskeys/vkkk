#pragma once

#include <filesystem>

#include "asset_mgr/mesh.h"
#include "utils/singleton.h"

namespace fs=std::filesystem;

namespace vkkk
{

class MeshMgr : public Singleton<MeshMgr> {
public:
    void load_file(const fs::path& path);

private:
    std::vector<Mesh>   meshes;
};

}