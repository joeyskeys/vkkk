
#include <memory>
#include <stdexcept>

#include <fmt/format.h>

#include "concepts/mesh.h"
//#include "vk_ins/cmd_buf.h"
//#include "vk_ins/pipeline_mgr.h"
// Legacy wrapper include not needed for Mesh data container.
// #include "vk_ins/vkabstraction.h"

namespace vkkk
{

Mesh::Mesh(const std::vector<VERT_COMP>& cs, bool idx)
    : comps(cs)
    , indexed(idx)
    , comp_size(0)
{
    for (const auto& comp : comps)
        comp_size += comp_sizes[comp];
}

Mesh::Mesh(const Mesh& m)
    : comps(m.comps)
    , indexed(m.indexed)
    , comp_size(m.comp_size)
    , vcnt(m.vcnt)
    , icnt(m.icnt)
    , loaded(m.loaded)
{
    if (loaded) {
        vbuf = new float[vcnt * comp_size];
        memcpy(vbuf, m.vbuf, vcnt * comp_size * sizeof(float));
        ibuf = new uint32_t[icnt * 3];
        memcpy(ibuf, m.ibuf, icnt * 3 * sizeof(uint32_t));
    }
}

Mesh::Mesh(Mesh&& m)
    : comps(std::move(m.comps))
    , indexed(m.indexed)
    , comp_size(m.comp_size)
    , vcnt(m.vcnt)
    , icnt(m.icnt)
    , loaded(m.loaded)
{
    if (loaded) {
        vbuf = m.vbuf;
        m.vbuf = nullptr;
        ibuf = m.ibuf;
        m.ibuf = nullptr;
        m.loaded = false;
    }
}

Mesh::~Mesh() {
    if (loaded) {
        delete[] vbuf;
        delete[] ibuf;
    }
}

void Mesh::load(aiMesh* mesh, bool interleaved) {
    vcnt = mesh->mNumVertices;
    icnt = mesh->mNumFaces;

    vbuf = new float[vcnt * comp_size];
    ibuf = new uint32_t[icnt * 3];
    strides.resize(comps.size());

    uint32_t prev = 0;
    for (int i = 0; i < comps.size(); ++i) {
        const auto& comp = comps[i];

        if (interleaved) {
            switch (comp) {
                case VERTEX: {
                    for (int i = 0; i < vcnt; ++i) {
                        vbuf[i * comp_size + prev    ] = mesh->mVertices[i].x;
                        vbuf[i * comp_size + prev + 1] = mesh->mVertices[i].y;
                        vbuf[i * comp_size + prev + 2] = mesh->mVertices[i].z;
                    }
                    break;
                }

                case NORMAL: {
                    for (int i = 0; i < vcnt; ++i) {
                        vbuf[i * comp_size + prev    ] = mesh->mNormals[i].x;
                        vbuf[i * comp_size + prev + 1] = mesh->mNormals[i].y;
                        vbuf[i * comp_size + prev + 2] = mesh->mNormals[i].z;
                    }
                    break;
                }

                case UV: {
                    for (int i = 0; i < vcnt; ++i) {
                        const auto uv = mesh->mTextureCoords[0][i];
                        vbuf[i * comp_size + prev    ] = uv.x;
                        vbuf[i * comp_size + prev + 1] = uv.y;
                    }
                    break;
                }

                case COLOR: {
                    for (int i = 0; i < vcnt; ++i) {
                        const auto vcolor = mesh->mColors[0][i];
                        vbuf[i * comp_size + prev    ] = vcolor.r;
                        vbuf[i * comp_size + prev + 1] = vcolor.g;
                        vbuf[i * comp_size + prev + 2] = vcolor.b;
                    }
                    break;
                }
            }
            strides[i] = prev;
            prev += comp_sizes[comp];

        }
        else {
            switch (comp) {
                case VERTEX: {
                    memcpy(vbuf + prev, mesh->mVertices, vcnt * comp_sizes[comp] * sizeof(float));
                    break;
                }
                case NORMAL: {
                    memcpy(vbuf + prev, mesh->mNormals, vcnt * comp_sizes[comp] * sizeof(float));
                    break;
                }
                case UV: {
                    memcpy(vbuf + prev, mesh->mTextureCoords[0], vcnt * comp_sizes[comp] * sizeof(float));
                    break;
                }
                case COLOR: {
                    memcpy(vbuf + prev, mesh->mColors[0], vcnt * comp_sizes[comp] * sizeof(float));
                    break;
                }
            }
            strides[i] = prev;
            prev += comp_sizes[comp] * vcnt;
        }
    }

    for (int i = 0; i < icnt; ++i) {
        ibuf[i * 3    ] = mesh->mFaces[i].mIndices[0];
        ibuf[i * 3 + 1] = mesh->mFaces[i].mIndices[1];
        ibuf[i * 3 + 2] = mesh->mFaces[i].mIndices[2];
    }

    loaded = true;
}

void Mesh::load(const uint32_t v, const char* vdata, const uint32_t vsize,
    const uint32_t i, const char* idata, const uint32_t isize)
{
    const auto vbuf_size = v * comp_size * sizeof(float);
    const auto ibuf_size = i * 3 * sizeof(uint32_t);

    if (vbuf_size != vsize) {
        throw std::length_error(fmt::format("Required vbuf size : {}, actual : {}",
            vbuf_size, vsize));
    }
    if (ibuf_size != isize) {
        throw std::length_error(fmt::format("Required ibuf size : {}, actual : {}",
            ibuf_size, isize));
    }

    vcnt = v;
    vbuf = new float[vcnt * comp_size];
    memcpy(vbuf, vdata, vbuf_size);

    icnt = i;
    ibuf = new uint32_t[icnt * 3];
    memcpy(ibuf, idata, ibuf_size);

    loaded = true;
}

void Mesh::unload() {
    vcnt = icnt = 0;
    delete[] vbuf;
    delete[] ibuf;
    loaded = false;
}

}