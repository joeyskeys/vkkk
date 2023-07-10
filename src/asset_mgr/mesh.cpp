#include "mesh.h"

#include <memory>
#include <stdexcept>

#include "vk_ins/vkabstraction.h"

namespace vkkk
{

Mesh::Mesh(VkWrappedInstance* i, const std::vector<VERT_COMP>& cs, bool indexed)
    : ins(i)
    , comps(cs)
    , indexed(indexed)
{}

Mesh::Mesh(const Mesh& b) {
    ins = b.ins;
    comps = b.comps;
    indexed = b.indexed;
    comp_size = b.comp_size;
    vcnt = b.vcnt;
    vbuf = std::make_unique<float[]>(vcnt * comp_size);
    memcpy(vbuf.get(), b.vbuf.get(), vcnt * comp_size * sizeof(float));
    icnt = b.icnt;
    ibuf = std::make_unique<uint32_t[]>(icnt * 3);
    memcpy(ibuf.get(), b.ibuf.get(), icnt * 3 * sizeof(uint32_t));
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
    vbuf = std::move(b.vbuf);
    icnt = b.icnt;
    ibuf = std::move(b.ibuf);
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
    if (gpu_loaded)
        unload_gpu();
}

void Mesh::load(aiMesh *mesh) {
    vcnt = mesh->mNumVertices;
    icnt = mesh->mNumFaces;
    comp_size = 0;
    for (const auto& comp : comps)
        comp_size += comp_sizes[comp];

    vbuf = std::make_unique<float[]>(vcnt * comp_size);
    ibuf = std::make_unique<uint32_t[]>(icnt * 3);

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

void Mesh::unload() {
    vcnt = icnt = 0;
    vbuf.reset();
    ibuf.reset();
    loaded = false;
}

void Mesh::load_gpu() {
    ins->create_vertex_buffer(vbuf.get(), vbuf_gpu, comp_size, vcnt);
    // * 3 means we're dealing with triangle
    ins->create_index_buffer(ibuf.get(), ibuf_gpu, icnt * 3);
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
    VkDescriptorSet* desc_set)
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