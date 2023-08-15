#pragma once

#include <filesystem>

#include "concepts/mesh.h"
#include "utils/singleton.h"

namespace fs=std::filesystem;

namespace vkkk
{

class VkWrappedInstance;

class MeshMgr : public Singleton<MeshMgr> {
private:
    MeshMgr(VkWrappedInstance* i);
    MeshMgr(const MeshMgr&) = delete;
    MeshMgr& operator= (const MeshMgr&) = delete;
    friend class Singleton<MeshMgr>;

public:
    Mesh* load_file(const fs::path &path, const std::vector<VERT_COMP>& cs);
    void add_box(const void *min, const void *max);

    inline void init(VkWrappedInstance* i) {
        ins = i;
    }

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
    std::vector<Mesh>   meshes;

private:
    VkWrappedInstance*  ins;
};

}