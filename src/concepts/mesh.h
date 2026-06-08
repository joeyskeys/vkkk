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
#include "vk_ins/types.h"

namespace vkkk
{

class VkWrappedInstance;

class Mesh {
public:
    Mesh(const std::vector<VERT_COMP>& cs, bool indexed=true);
    Mesh(const Mesh&);
    Mesh(Mesh&&);
    ~Mesh();

    Mesh& operator=(const Mesh&) = default;

    void load(aiMesh *mesh, bool interleaved=true);

    void load(const uint32_t, const char*, const uint32_t, const uint32_t, const char*,
        const uint32_t);

    void unload();

    std::vector<VERT_COMP>      comps;
    bool                        indexed = true;
    uint32_t                    comp_size = 0;
    uint32_t                    vcnt = 0;
    float*                      vbuf = nullptr;
    std::vector<uint32_t>       strides;
    uint32_t                    icnt = 0;
    uint32_t*                   ibuf = nullptr;
    bool                        loaded = false;
};

}