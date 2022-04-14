#pragma once

#include <cstdint>
#include <concepts>
#include <type_traits>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <vulkan/vulkan.h>

namespace vkkk
{

template <typename T, uint32_t D>
concept VertexConstraint = requires(T t, uint32_t d) {
    std::floating_point<T>;
    2 <= d && d <= 3;
};

template <uint32_t D>
concept ThridComponent = D == 3;

class Vertex {
public:
    static VkVertexInputBindingDescription get_binding_description(uint32 binding) {
        VkVertexInputBindingDescription des{};
        des.binding = binding;
        des.stride = sizeof(Vertex);
        des.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return res;
    }

    static auto get_attr_descriptions(uint32_t binding, uint32_t loc) {
        std::array<VkVertexInputAttributeDescription, 1> des{};

        des[0].binding = binding;
        des[0].location = loc;
        des[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        des[0].offset = 0;

        /*
        des[1].binding = 0;
        des[1].location = 1;
        des[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        des[1].offset = offsetof(VertexTmp, color);

        des[2].binding = 0;
        des[2].location = 2;
        des[2].format = VK_FORMAT_R32G32_SFLOAT;
        des[2].offset = offsetof(VertexTmp, uv);
        */

        return des;
    }

public:
    union {
        std::array<float, 3> arr;
        struct {
            float x;
            float y;
            float z;
        };
        glm::vec3 pos;
    };
};

class VertexUV {
public:
    static VkVertexInputBindingDescription get_binding_description(uint32_t binding) {
        VkVertexInputBindingDescription des{};
        des.binding = binding;
        des.stride = sizeof(VertexUV);
        des.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return res;
    }

    static auto get_attr_descriptions(uint32_t binding, uint32_t loc_pos, uint32_t loc_uv) {
        std::array<VkVertexInputAttributeDescription, 2> des{};

        des[0].binding = binding;
        des[0].location = loc_pos;
        des[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        des[0].offset = offsetof(VertexUV, pos);

        des[1].binding = binding;
        des[1].location = loc_uv;
        des[1].format = VK_FORMAT_R32G32_SFLOAT;
        des[1].offset = offsetof(VertexUV, uv);

        return des;
    }

public:
    union {
        std::array<float, 5> arr;
        struct {
            float x;
            float y;
            float z;
            float u;
            float v;
        };
        struct {
            glm::vec3 pos;
            glm::vec2 uv;
        };
    };
};

class VertexUVColor {
public:
    static VkVertexInputBindingDescription get_binding_description(uint32 binding) {
        VkVertexInputBindingDescription des{};
        des.binding = binding;
        des.stride = sizeof(VertexColorUV);
        des.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return res;
    }

    static auto get_attr_descriptions(uint32_t binding, uint32_t loc_pos, uint32_t loc_color, uint32_t loc_uv) {
        std::array<VkVertexInputAttributeDescription, 3> des{};

        des[0].binding = binding;
        des[0].location = loc_pos;
        des[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        des[0].offset = 0;

        des[1].binding = binding;
        des[1].location = loc_uv;
        des[1].format = VK_FORMAT_R32G32_SFLOAT;
        des[1].offset = offsetof(VertexColorUV, uv);

        des[2].binding = binding;
        des[2].location = loc_color;
        des[2].format = VK_FORMAT_R32G32B32_SFLOAT;
        des[2].offset = offsetof(VertexColorUV, color);

        return des;
    }

public:
    union {
        std::array<float, 8> arr;
        struct {
            float x;
            float y;
            float z;
            float u;
            float v;
            float r;
            float g;
            float b;
        };
        struct {
            glm::vec3 pos;
            glm::vec2 uv;
            glm::vec3 color;
        };
    };
};

// POS BIT is a "must have"
constexpr static int COLOR_BIT = 1;
constexpr static int UV_BIT = 1 << 1;

class Mesh {
public:
    Mesh(uint32_t flag, bool indexed=true);
    virtual ~Mesh();

    void load(aiMesh *mesh);

private:
    uint32_t                    comp_flag = 0;
    bool                        indexed = true;
    uint32_t                    comp_size;
    uint32_t                    vcnt;
    std::unique_ptr<float[]>    vbuf = nullptr;
    uint32_t                    icnt;
    std::unique_ptr<uint32_t[]> ibuf = nullptr;
};

}