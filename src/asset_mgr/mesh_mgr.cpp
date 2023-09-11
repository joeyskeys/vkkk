#include <iostream>

#include <fmt/format.h>

#include "asset_mgr/mesh_mgr.h"
#include "vk_ins/vkabstraction.h"

namespace vkkk
{

MeshMgrDeprecated::MeshMgrDeprecated(VkWrappedInstance* i)
    : ins(i)
{}

void MeshMgrDeprecated::process_node(aiNode *node, const aiScene *scene,
    const std::vector<VERT_COMP>& cs)
{
    for (int i = 0; i < node->mNumMeshes; ++i) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        MeshDeprecated m{ins, cs};
        m.load(mesh);
        meshes.emplace_back(std::move(m));
    }

    for (int i = 0; i < node->mNumChildren; ++i)
        process_node(node->mChildren[i], scene, cs);
}

MeshDeprecated* MeshMgrDeprecated::load_file(const fs::path& path, const std::vector<VERT_COMP>& cs) {
    if (!fs::exists(fs::absolute(path))) {
        std::cerr << "file : " << path << "does not exist" << std::endl;
        throw std::runtime_error("model file does not exist");
    }

    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(path.string().c_str(), aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        return nullptr;

    auto idx = meshes.size();
    process_node(scene->mRootNode, scene, cs);
    return &meshes[idx];
}

void MeshMgrDeprecated::load(const std::vector<VERT_COMP>& cs, const uint32_t v, const char* vbuf,
    const uint32_t vs, const uint32_t i, const char* ibuf, const uint32_t is)
{
    MeshDeprecated m{ins, cs};
    m.load(v, vbuf, vs, i, ibuf, is);
    meshes.emplace_back(std::move(m));
}

void MeshMgr::process_node(const std::string& name, aiNode *node, const aiScene *scene,
    const std::vector<VERT_COMP>& cs)
{
    for (int i = 0; i < node->mNumMeshes; ++i) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        Mesh m{cs};
        m.load(mesh);
        meshes.emplace(name, std::move(m));
    }

    for (int i = 0; i < node->mNumChildren; ++i)
        process_node(name, node->mChildren[i], scene, cs);
}

void MeshMgr::load_file(const fs::path& path, const std::string& name,
    const std::vector<VERT_COMP>& cs)
{
    if (!fs::exists(fs::absolute(path))) {
        std::cerr << "file : " << path << "does not exist" << std::endl;
        throw std::runtime_error("model file does not exist");
    }

    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(path.string().c_str(), aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        return;

    auto idx = meshes.size();
    process_node(name, scene->mRootNode, scene, cs);
}

void MeshMgr::load(const std::string& name, const std::vector<VERT_COMP>& cs,
    const uint32_t v, const char* vbuf, const uint32_t vs, const uint32_t i,
    const char* ibuf, const uint32_t is)
{
    Mesh m{cs};
    m.load(v, vbuf, vs, i, ibuf, is);
    meshes.emplace(name, std::move(m));
}

void MeshMgr::upload_gpu(VkWrappedInstance* ins, const std::string& name) const {
    auto found = meshes.find(name);
    if (found == meshes.end()) {
        std::cout << "Mesh with name " << name << " not found.." << std::endl;
        return;
    }
    ins->load_mesh(name, found->second);
}

}