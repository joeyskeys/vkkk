#pragma once

#include <cstdint>
#include <concepts>
#include <type_traits>

namespace vkkk
{

template <typename T, uint32_t D>
concept VertexConstraint = requires(T t, uint32_t d) {
    std::floating_point<T>;
    2 <= d && d <= 3;
};

template <uint32_t D>
concept ThridComponent = D == 3;

class Vertex2 {
public:

public:
    union {
        std::array<float, 3> arr;
        struct {
            float x;
            float y;
            float z;
        };
    };
};

}