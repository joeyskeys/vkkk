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

#include "vk_ins/types.h"

namespace vkkk
{

class VkWrappedInstance;

class Mesh {
public:
    Mesh(VkWrappedInstance*, const std::vector<VERT_COMP>&, bool indexed=true);
    Mesh(const Mesh&);
    Mesh(Mesh&&);
    virtual ~Mesh();

    void load(aiMesh *mesh);
    void unload();
    void load_gpu();
    void unload_gpu();

    void emit_draw_cmd(VkCommandBuffer, VkPipelineLayout, const VkDescriptorSet*);

public:
    VkWrappedInstance*          ins;
    std::vector<VERT_COMP>      comps;
    bool                        indexed = true;
    uint32_t                    comp_size;
    uint32_t                    vcnt;
    std::unique_ptr<float[]>    vbuf = nullptr;
    uint32_t                    icnt;
    std::unique_ptr<uint32_t[]> ibuf = nullptr;
    bool                        loaded = false;

    VkBuffer                    vbuf_gpu;
    VkDeviceMemory              vbuf_memo;
    VkBuffer                    ibuf_gpu;
    VkDeviceMemory              ibuf_memo;
    bool                        gpu_loaded = false;
};

}