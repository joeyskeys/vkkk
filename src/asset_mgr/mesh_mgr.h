#pragma once

#include <filesystem>

#include "concepts/mesh.h"
#include "utils/singleton.h"

namespace fs=std::filesystem;

namespace vkkk
{

class VkWrappedInstance;

class MeshMgrDeprecated : public Singleton<MeshMgrDeprecated> {
private:
    MeshMgrDeprecated(VkWrappedInstance* i);
    MeshMgrDeprecated(const MeshMgrDeprecated&) = delete;
    MeshMgrDeprecated& operator= (const MeshMgrDeprecated&) = delete;
    friend class Singleton<MeshMgrDeprecated>;

public:
    Mesh* load_file(const fs::path &path, const std::vector<VERT_COMP>& cs);
    void  load(const std::vector<VERT_COMP>&, const uint32_t, const char*,
        const uint32_t, const uint32_t, const char*, const uint32_t);
    void add_box(const void *min, const void *max);

    inline void pour_into_gpu() {
        for (auto& mesh : meshes)
            mesh.load_gpu();
    }

    inline void free_gpu_resources() {
        for (auto& mesh : meshes)
            if (mesh.gpu_loaded)
                mesh.unload_gpu();
    }

    inline void emit_draw_cmds(VkCommandBuffer cmd_buf, VkPipelineLayout ppl_layout,
        const VkDescriptorSet* sets)
    {
        for (auto& mesh : meshes)
            mesh.emit_draw_cmd(cmd_buf, ppl_layout, sets);
    }

private:
    void process_node(aiNode *node, const aiScene *scene,
        const std::vector<VERT_COMP>& cs);

public:
    std::vector<MeshDeprecated>   meshes;

private:
    VkWrappedInstance*  ins;
};

class MeshMgr : public Singleton<MeshMgr> {
private:
    MeshMgr() {}
    MeshMgr(const MeshMgr&) = delete;
    MeshMgr& operator= (const MeshMgr&) = delete;
    friend class Singleton<MeshMgr>;

    void process_node(aiNode* node, const aiScene* scene,
        const std::vector<VERT_COMP>& cs);

public:
    void load_file(const fs::path&, const std::vector<VERT_COMP>&);
    void load(const std::string&, const std::vector<VERT_COMP>&, const uint32_t,
        const char*, const uint32_t, const uint32_t, const char*, const uint32_t);
    
    void upload_gpu(VkWrappedInstance*, const std::string&) const;

private:
    std::unordered_map<std::string, Mesh>   meshes;
};

}