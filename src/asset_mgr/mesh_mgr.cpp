
#include "mesh_mgr.h"

static void process_node(aiNode* node, const aiScene* scene) {
    for (int i = 0; i < node->mNumMeshes; ++i) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        Mesh m{};
        m.load(mesh);
        meshes.emplace_back(std::move(m));
    }

    for (int i = 0; i < node->mNumChildren; ++i)
        process_node(node->mChildren[i], scene);
}

void MeshMgr::load_file(const fs::path& path) {
    if (!fs::exists(fs::absolute(path))) {
        std::cout << "file : " << path << "does not exist" << std::endl;
        return;
    }

    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        return;

    process_node(scene->mRootNode, scene);
}