#include "mesh.h"

Mesh::Mesh(uint32_t flag, bool indexed)
    : comp_flag(flag)
    , indexed(indexed)
{}

Mesh::~Mesh() {
    if (vbuf)
        delete vbuf[];

    if (ibuf)
        delete ibuf[];
}

void Mesh::load(aiMesh *mesh) {
    uint32_t vcnt = mesh->mNumVertices;
    uint32_t icnt = mesh->mNumFaces;
    uint32_t comp_size = 3 + 3 * (comp_flag & COLOR_BIT) + 2 * (comp_flag & UV_BIT >> 1);

    vbuf = new float[vcnt * comp_size];
    ibuf = new uint32_t[icnt * 3];

    for (uint i = 0; i < vcnt; ++i) {
        vbuf[i * comp_size    ] = mesh->mVertices[i].x;
        vbuf[i * comp_size + 1] = mesh->mVertices[i].y;
        vbuf[i * comp_size + 2] = mesh->mVertices[i].z;
    }

    for (int i = 0; i < icnt; ++i) {
        ibuf[i * 3    ] = mesh->mFaces[i].mIndices[0];
        ibuf[i * 3 + 1] = mesh->mFaces[i].mIndices[1];
        ibuf[i * 3 + 2] = mesh->mFaces[i].mIndices[2];
    }
}