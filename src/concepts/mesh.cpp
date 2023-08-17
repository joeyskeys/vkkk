
#include <memory>
#include <stdexcept>

#include <fmt/format.h>

#include "concepts/mesh.h"
#include "vk_ins/vkabstraction.h"

namespace vkkk
{

Mesh::Mesh(VkWrappedInstance* i, const std::vector<VERT_COMP>& cs, bool indexed)
    : ins(i)
    , comps(cs)
    , indexed(indexed)
{
    comp_size = 0;
    for (const auto& comp : comps)
        comp_size += comp_sizes[comp];
}

Mesh::Mesh(const Mesh& b) {
    ins = b.ins;
    comps = b.comps;
    indexed = b.indexed;
    comp_size = b.comp_size;
    vcnt = b.vcnt;
    vbuf = new float[vcnt * comp_size];
    memcpy(vbuf, b.vbuf, vcnt * comp_size * sizeof(float));
    icnt = b.icnt;
    ibuf = new uint32_t[icnt * 3];
    memcpy(ibuf, b.ibuf, icnt * 3 * sizeof(uint32_t));
    loaded = b.loaded;
    gpu_loaded = b.gpu_loaded;
    if (gpu_loaded)
        load_gpu();
}

Mesh::Mesh(Mesh&& b) {
    ins = b.ins;
    comps = std::move(b.comps);
    indexed = b.indexed;
    comp_size = b.comp_size;
    vcnt = b.vcnt;
    vbuf = b.vbuf;
    icnt = b.icnt;
    ibuf = b.ibuf;
    vbuf_gpu = b.vbuf_gpu;
    vbuf_memo = b.vbuf_memo;
    ibuf_gpu = b.ibuf_gpu;
    ibuf_memo = b.ibuf_memo;
    loaded = b.loaded;
    gpu_loaded = b.gpu_loaded;
    b.loaded = false;
    b.gpu_loaded = false;
}

Mesh::~Mesh() {
    if (loaded)
        unload();
    //if (gpu_loaded)
        //unload_gpu();
}

void Mesh::load(aiMesh *mesh) {
    vcnt = mesh->mNumVertices;
    icnt = mesh->mNumFaces;
    /*
    comp_size = 0;
    for (const auto& comp : comps)
        comp_size += comp_sizes[comp];
    */

    vbuf = new float[vcnt * comp_size];
    ibuf = new uint32_t[icnt * 3];

    uint32_t prev = 0;
    for (const auto& comp : comps) {
        switch (comp) {
            case VERTEX: {
                for (int i = 0; i < vcnt; ++i) {
                    vbuf[i * comp_size + prev    ] = mesh->mVertices[i].x;
                    vbuf[i * comp_size + prev + 1] = mesh->mVertices[i].y;
                    vbuf[i * comp_size + prev + 2] = mesh->mVertices[i].z;
                }
                prev += 3;
                break;
            }
            case NORMAL: {
                for (int i = 0; i < vcnt; ++i) {
                    vbuf[i * comp_size + prev    ] = mesh->mNormals[i].x;
                    vbuf[i * comp_size + prev + 1] = mesh->mNormals[i].y;
                    vbuf[i * comp_size + prev + 2] = mesh->mNormals[i].z;
                }
                prev += 3;
                break;
            }
            case UV: {
                for (int i = 0; i < vcnt; ++i) {
                    const auto uv = mesh->mTextureCoords[0][i];
                    vbuf[i * comp_size + prev    ] = uv.x;
                    vbuf[i * comp_size + prev + 1] = uv.y;
                }
                prev += 2;
                break;
            }
            case COLOR: {
                for (int i = 0; i < vcnt; ++i) {
                    const auto vcolor = mesh->mColors[0][i];
                    vbuf[i * comp_size + prev    ] = vcolor.r;
                    vbuf[i * comp_size + prev + 1] = vcolor.g;
                    vbuf[i * comp_size + prev + 2] = vcolor.b;
                }
                prev += 3;
                break;
            }
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
    delete [] vbuf;
    delete [] ibuf;
    loaded = false;
}

void Mesh::set_view(uint32_t v, void* vview, uint32_t i, void* iview) {
    // Do not owning the data
    vcnt = v;
    vbuf = reinterpret_cast<float*>(vview);
    icnt = i;
    ibuf = reinterpret_cast<uint32_t*>(iview);
}

void Mesh::load_gpu() {
    ins->create_vertex_buffer(vbuf, vbuf_gpu, vbuf_memo, comp_size, vcnt);
    // * 3 means we're dealing with triangle
    ins->create_index_buffer(ibuf, ibuf_gpu, ibuf_memo, icnt * 3);
    gpu_loaded = true;
}

void Mesh::unload_gpu() {
    vkDestroyBuffer(ins->get_device(), ibuf_gpu, nullptr);
    vkFreeMemory(ins->get_device(), ibuf_memo, nullptr);
    vkDestroyBuffer(ins->get_device(), vbuf_gpu, nullptr);
    vkFreeMemory(ins->get_device(), vbuf_memo, nullptr);
    gpu_loaded = false;
}

void Mesh::emit_draw_cmd(VkCommandBuffer cmd_buf, VkPipelineLayout ppl_layout,
    const VkDescriptorSet* desc_set)
{
    VkBuffer bufs[] = {vbuf_gpu};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd_buf, 0, 1, bufs, offsets);
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, ppl_layout,
        0, 1, desc_set, 0, nullptr);
    vkCmdBindIndexBuffer(cmd_buf, ibuf_gpu, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd_buf, icnt * 3, 1, 0, 0, 0);
}

}