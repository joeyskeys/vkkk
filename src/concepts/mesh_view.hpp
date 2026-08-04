#pragma once

#include <cstdint>
#include <vector>

#include "concepts/mesh.h"
#include "vk_ins/types.h"

namespace vkkk
{

// Non-owning view over interleaved mesh data described by VERT_COMP.
// Layout matches Mesh: float vertex packing via comp_sizes; icnt is triangle count.
class MeshView {
public:
    MeshView() = default;

    MeshView(std::vector<VERT_COMP> comps, const float* vbuf, uint32_t vcnt,
        const uint32_t* ibuf = nullptr, uint32_t icnt = 0, bool indexed = true);

    explicit MeshView(const Mesh& mesh);

    bool empty() const;

    uint32_t vertex_float_count() const;
    uint32_t vertex_bytes() const;
    uint32_t index_count() const;
    uint32_t index_bytes() const;

    std::vector<VERT_COMP> comps;
    bool indexed = true;
    uint32_t comp_size = 0;
    uint32_t vcnt = 0;
    const float* vbuf = nullptr;
    uint32_t icnt = 0;
    const uint32_t* ibuf = nullptr;
};

} // namespace vkkk
