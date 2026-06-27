#pragma once

#include <filesystem>
#include <unordered_map>

#include <glm/mat4x4.hpp>

#include "concepts/line.h"
#include "concepts/mesh.h"
#include "utils/singleton.h"

namespace fs = std::filesystem;

namespace vkkk
{

class DrawableMgr : public Singleton<DrawableMgr> {
private:
    DrawableMgr() {}
    DrawableMgr(const DrawableMgr&) = delete;
    DrawableMgr& operator= (const DrawableMgr&) = delete;
    friend class Singleton<DrawableMgr>;

    void process_node(const std::string&, aiNode* node, const aiScene* scene,
        const std::vector<VERT_COMP>& cs);

public:
    // Mesh management
    void load_file(const fs::path&, const std::string&, const std::vector<VERT_COMP>&);
    void load_mesh(const std::string&, const std::vector<VERT_COMP>&, const uint32_t,
        const char*, const uint32_t, const uint32_t, const char*, const uint32_t);
    void add_plane(const std::string&, const std::vector<VERT_COMP>&, float size=1.0f);
    void add_cube(const std::string&, const std::vector<VERT_COMP>&, float size=1.0f);
    void add_sphere(const std::string&, const std::vector<VERT_COMP>&, float radius=0.5f,
        uint32_t stacks=16, uint32_t slices=32);

    // Line management
    void add_line(const std::string&, const std::vector<VERT_COMP>&,
        const glm::vec3&, const glm::vec3&);

    // Legacy path (old VkWrappedInstance wrapper). Temporarily disabled while testing Context.
    // void upload_gpu(VkWrappedInstance*, const std::string&) const;
    const Mesh* find_mesh(const std::string& name) const;

private:
    std::unordered_map<std::string, Mesh>   meshes;
    std::unordered_map<std::string, Line>   lines;
};

} // namespace vkkk
