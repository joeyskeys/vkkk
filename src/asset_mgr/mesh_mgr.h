#pragma once

#include <filesystem>
#include <unordered_map>

#include "concepts/mesh.h"
#include "utils/singleton.h"

namespace fs=std::filesystem;

namespace vkkk
{

class VkWrappedInstance;

class MeshMgr : public Singleton<MeshMgr> {
private:
    MeshMgr() {}
    MeshMgr(const MeshMgr&) = delete;
    MeshMgr& operator= (const MeshMgr&) = delete;
    friend class Singleton<MeshMgr>;

    void process_node(const std::string&, aiNode* node, const aiScene* scene,
        const std::vector<VERT_COMP>& cs);

public:
    void load_file(const fs::path&, const std::string&, const std::vector<VERT_COMP>&);
    void load(const std::string&, const std::vector<VERT_COMP>&, const uint32_t,
        const char*, const uint32_t, const uint32_t, const char*, const uint32_t);
    void add_cube(const std::string&, const std::vector<VERT_COMP>&, float size=1.0f);
    void add_sphere(const std::string&, const std::vector<VERT_COMP>&, float radius=0.5f,
        uint32_t stacks=16, uint32_t slices=32);
    
    void upload_gpu(VkWrappedInstance*, const std::string&) const;

private:
    std::unordered_map<std::string, Mesh>   meshes;
};

}