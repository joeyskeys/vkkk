#include <iostream>

#include <fmt/format.h>

#include "asset_mgr/mesh_mgr.h"

namespace vkkk
{

void MeshMgr::process_node(aiNode *node, const aiScene *scene, uint32_t flag) {
    for (int i = 0; i < node->mNumMeshes; ++i) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        Mesh m{flag};
        m.load(mesh);
        meshes.emplace_back(std::move(m));
    }

    for (int i = 0; i < node->mNumChildren; ++i)
        process_node(node->mChildren[i], scene, flag);
}

void MeshMgr::load_file(const fs::path& path, uint32_t flag) {
    if (!fs::exists(fs::absolute(path))) {
        std::cerr << "file : " << path << "does not exist" << std::endl;
        throw std::runtime_error("model file does not exist");
    }

    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(path.string().c_str(), aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        return;

    process_node(scene->mRootNode, scene, flag);
}

}