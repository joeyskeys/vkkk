#include <iostream>
#include <array>
#include <cmath>
#include <vector>

#include <fmt/format.h>

#include "asset_mgr/mesh_mgr.h"
#include "vk_ins/vkabstraction.h"

namespace vkkk
{

namespace
{

constexpr float PI = 3.14159265358979323846f;

struct ProcVertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec3 color;
};

void append_vertex(std::vector<float>& out, const std::vector<VERT_COMP>& comps,
    const ProcVertex& v)
{
    for (const auto comp : comps) {
        switch (comp) {
            case VERTEX:
                out.push_back(v.pos.x);
                out.push_back(v.pos.y);
                out.push_back(v.pos.z);
                break;
            case NORMAL:
                out.push_back(v.normal.x);
                out.push_back(v.normal.y);
                out.push_back(v.normal.z);
                break;
            case UV:
                out.push_back(v.uv.x);
                out.push_back(v.uv.y);
                break;
            case COLOR:
                out.push_back(v.color.x);
                out.push_back(v.color.y);
                out.push_back(v.color.z);
                break;
        }
    }
}

} // namespace

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

void MeshMgr::add_cube(const std::string& name, const std::vector<VERT_COMP>& cs, float size) {
    const float h = size * 0.5f;

    const std::array<ProcVertex, 24> vertices = {{
        {{-h, -h,  h}, { 0,  0,  1}, {0, 0}, {1, 1, 1}},
        {{ h, -h,  h}, { 0,  0,  1}, {1, 0}, {1, 1, 1}},
        {{ h,  h,  h}, { 0,  0,  1}, {1, 1}, {1, 1, 1}},
        {{-h,  h,  h}, { 0,  0,  1}, {0, 1}, {1, 1, 1}},

        {{ h, -h, -h}, { 0,  0, -1}, {0, 0}, {1, 1, 1}},
        {{-h, -h, -h}, { 0,  0, -1}, {1, 0}, {1, 1, 1}},
        {{-h,  h, -h}, { 0,  0, -1}, {1, 1}, {1, 1, 1}},
        {{ h,  h, -h}, { 0,  0, -1}, {0, 1}, {1, 1, 1}},

        {{-h, -h, -h}, {-1,  0,  0}, {0, 0}, {1, 1, 1}},
        {{-h, -h,  h}, {-1,  0,  0}, {1, 0}, {1, 1, 1}},
        {{-h,  h,  h}, {-1,  0,  0}, {1, 1}, {1, 1, 1}},
        {{-h,  h, -h}, {-1,  0,  0}, {0, 1}, {1, 1, 1}},

        {{ h, -h,  h}, { 1,  0,  0}, {0, 0}, {1, 1, 1}},
        {{ h, -h, -h}, { 1,  0,  0}, {1, 0}, {1, 1, 1}},
        {{ h,  h, -h}, { 1,  0,  0}, {1, 1}, {1, 1, 1}},
        {{ h,  h,  h}, { 1,  0,  0}, {0, 1}, {1, 1, 1}},

        {{-h,  h,  h}, { 0,  1,  0}, {0, 0}, {1, 1, 1}},
        {{ h,  h,  h}, { 0,  1,  0}, {1, 0}, {1, 1, 1}},
        {{ h,  h, -h}, { 0,  1,  0}, {1, 1}, {1, 1, 1}},
        {{-h,  h, -h}, { 0,  1,  0}, {0, 1}, {1, 1, 1}},

        {{-h, -h, -h}, { 0, -1,  0}, {0, 0}, {1, 1, 1}},
        {{ h, -h, -h}, { 0, -1,  0}, {1, 0}, {1, 1, 1}},
        {{ h, -h,  h}, { 0, -1,  0}, {1, 1}, {1, 1, 1}},
        {{-h, -h,  h}, { 0, -1,  0}, {0, 1}, {1, 1, 1}},
    }};

    const std::array<uint32_t, 36> indices = {{
        0, 1, 2, 0, 2, 3,
        4, 5, 6, 4, 6, 7,
        8, 9, 10, 8, 10, 11,
        12, 13, 14, 12, 14, 15,
        16, 17, 18, 16, 18, 19,
        20, 21, 22, 20, 22, 23
    }};

    std::vector<float> packed_vertices;
    packed_vertices.reserve(vertices.size() * Mesh(cs).comp_size);
    for (const auto& v : vertices)
        append_vertex(packed_vertices, cs, v);

    Mesh m{cs};
    m.load(
        static_cast<uint32_t>(vertices.size()),
        reinterpret_cast<const char*>(packed_vertices.data()),
        static_cast<uint32_t>(packed_vertices.size() * sizeof(float)),
        static_cast<uint32_t>(indices.size() / 3),
        reinterpret_cast<const char*>(indices.data()),
        static_cast<uint32_t>(indices.size() * sizeof(uint32_t))
    );
    meshes.insert_or_assign(name, std::move(m));
}

void MeshMgr::add_sphere(const std::string& name, const std::vector<VERT_COMP>& cs,
    float radius, uint32_t stacks, uint32_t slices)
{
    if (stacks < 2) stacks = 2;
    if (slices < 3) slices = 3;

    const uint32_t vertex_count = (stacks + 1) * (slices + 1);
    std::vector<ProcVertex> vertices;
    vertices.reserve(vertex_count);

    for (uint32_t i = 0; i <= stacks; ++i) {
        const float v = static_cast<float>(i) / static_cast<float>(stacks);
        const float phi = v * PI;
        const float y = std::cos(phi);
        const float r = std::sin(phi);

        for (uint32_t j = 0; j <= slices; ++j) {
            const float u = static_cast<float>(j) / static_cast<float>(slices);
            const float theta = u * 2.0f * PI;
            const float x = r * std::cos(theta);
            const float z = r * std::sin(theta);

            vertices.push_back({
                {x * radius, y * radius, z * radius},
                {x, y, z},
                {u, 1.0f - v},
                {1.0f, 1.0f, 1.0f}
            });
        }
    }

    std::vector<uint32_t> indices;
    indices.reserve(stacks * slices * 6);
    const uint32_t row = slices + 1;
    for (uint32_t i = 0; i < stacks; ++i) {
        for (uint32_t j = 0; j < slices; ++j) {
            const uint32_t a = i * row + j;
            const uint32_t b = a + row;
            const uint32_t c = a + 1;
            const uint32_t d = b + 1;

            indices.push_back(a);
            indices.push_back(b);
            indices.push_back(c);
            indices.push_back(c);
            indices.push_back(b);
            indices.push_back(d);
        }
    }

    std::vector<float> packed_vertices;
    packed_vertices.reserve(vertices.size() * Mesh(cs).comp_size);
    for (const auto& v : vertices)
        append_vertex(packed_vertices, cs, v);

    Mesh m{cs};
    m.load(
        static_cast<uint32_t>(vertices.size()),
        reinterpret_cast<const char*>(packed_vertices.data()),
        static_cast<uint32_t>(packed_vertices.size() * sizeof(float)),
        static_cast<uint32_t>(indices.size() / 3),
        reinterpret_cast<const char*>(indices.data()),
        static_cast<uint32_t>(indices.size() * sizeof(uint32_t))
    );
    meshes.insert_or_assign(name, std::move(m));
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