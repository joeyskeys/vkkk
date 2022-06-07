#include "mesh.h"

#include <memory>
#include <stdexcept>

namespace vkkk
{

Mesh::Mesh(uint32_t flag, bool indexed)
    : comp_flag(flag)
    , indexed(indexed)
{}

Mesh::Mesh(const Mesh& b) {
    comp_flag = b.comp_flag;
    indexed = b.indexed;
    comp_size = b.comp_size;
    vcnt = b.vcnt;
    vbuf = std::make_unique<float[]>(vcnt * comp_size);
    memcpy(vbuf.get(), b.vbuf.get(), vcnt * comp_size * sizeof(float));
    icnt = icnt;
    ibuf = std::make_unique<uint32_t[]>(icnt * 3);
    memcpy(ibuf.get(), b.ibuf.get(), icnt * 3 * sizeof(uint32_t));
}

Mesh::Mesh(Mesh&& b) {
    comp_flag = b.comp_flag;
    indexed = b.indexed;
    comp_size = b.comp_size;
    vcnt = b.vcnt;
    vbuf = std::move(b.vbuf);
    icnt = icnt;
    ibuf = std::move(b.ibuf);
}

void Mesh::load(aiMesh *mesh) {
    vcnt = mesh->mNumVertices;
    icnt = mesh->mNumFaces;
    comp_size = 3 + 3 * (comp_flag & COLOR_BIT) + 2 * (comp_flag & UV_BIT >> 1);

    vbuf = std::make_unique<float[]>(vcnt * comp_size);
    ibuf = std::make_unique<uint32_t[]>(icnt * 3);

    for (uint32_t i = 0; i < vcnt; ++i) {
        vbuf[i * comp_size    ] = mesh->mVertices[i].x;
        vbuf[i * comp_size + 1] = mesh->mVertices[i].y;
        vbuf[i * comp_size + 2] = mesh->mVertices[i].z;
    }

    if (comp_flag & UV_BIT) {
        // Assimp support multiple uv sets, but we only check for
        // the first set
        if (!mesh->HasTextureCoords(0))
            throw std::runtime_error("Mesh doesn't have UVs");

        for (uint32_t i = 0; i < vcnt; ++i) {
            auto uv = mesh->mTextureCoords[0][i];
            vbuf[i * comp_size + 3] = uv.x;
            vbuf[i * comp_size + 4] = uv.y;
        }
    }

    if (comp_flag & COLOR_BIT) {
        // Also supports multiple channels of vertex colors
        if (!mesh->HasVertexColors(0))
            throw std::runtime_error("Mesh doesn't have Vertex Colors");

        for (uint32_t i = 0; i < vcnt; ++i) {
            auto vcolor = mesh->mColors[0][i];
            vbuf[i * comp_size + 5] = vcolor.r;
            vbuf[i * comp_size + 6] = vcolor.g;
            vbuf[i * comp_size + 7] = vcolor.b;
        }
    }

    for (int i = 0; i < icnt; ++i) {
        ibuf[i * 3    ] = mesh->mFaces[i].mIndices[0];
        ibuf[i * 3 + 1] = mesh->mFaces[i].mIndices[1];
        ibuf[i * 3 + 2] = mesh->mFaces[i].mIndices[2];
    }
}

}