#pragma once

#include <array>
#include <cstdint>
#include <concepts>
#include <functional>
#include <memory>
#include <type_traits>
#include <tuple>
#include <vector>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "vk_ins/cmd_buf.h"
#include "vk_ins/pipeline_mgr.h"
#include "vk_ins/types.h"

namespace vkkk
{

class VkWrappedInstance;

class MeshDeprecated {
public:
    MeshDeprecated(VkWrappedInstance*, const std::vector<VERT_COMP>&, bool indexed=true);
    MeshDeprecated(const MeshDeprecated&);
    MeshDeprecated(MeshDeprecated&&);
    virtual ~MeshDeprecated();

    void load(aiMesh *mesh);
    void load(const uint32_t, const char*, const uint32_t, const uint32_t, const char*,
        const uint32_t);
    void unload();
    void set_view(uint32_t v, void* vview, uint32_t i, void* iview);
    void load_gpu();
    void unload_gpu();

    void emit_draw_cmd(VkCommandBuffer, VkPipelineLayout, const VkDescriptorSet*);
    void emit_draw_cmd(CommandBuffers&, const uint32_t, PipelineMgr&, const std::string&);

public:
    VkWrappedInstance*          ins;
    std::vector<VERT_COMP>      comps;
    bool                        indexed = true;
    uint32_t                    comp_size;
    uint32_t                    vcnt;
    float*                      vbuf = nullptr;
    uint32_t                    icnt;
    uint32_t*                   ibuf = nullptr;
    bool                        loaded = false;

    VkBuffer                    vbuf_gpu;
    VkDeviceMemory              vbuf_memo;
    VkBuffer                    ibuf_gpu;
    VkDeviceMemory              ibuf_memo;
    bool                        gpu_loaded = false;
};

class Mesh {
public:
    Mesh(const std::vector<VERT_COMP>& cs, bool indexed=true);
    Mesh(const Mesh&);
    Mesh(Mesh&&);
    ~Mesh();

    void load(aiMesh *mesh);
    void load(const uint32_t, const char*, const uint32_t, const uint32_t, const char*,
        const uint32_t);
    void unload();

    std::vector<VERT_COMP>      comps;
    bool                        indexed = true;
    uint32_t                    comp_size = 0;
    uint32_t                    vcnt = 0;
    float*                      vbuf = nullptr;
    uint32_t                    icnt = 0;
    uint32_t*                   ibuf = nullptr;
    bool                        loaded = false;
};

}