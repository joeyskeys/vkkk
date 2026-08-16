#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "vk_ins/types.h"

namespace vkkk
{

// CPU-side indexed point-list data. Use a pipeline with ePointList when drawing.
class Points {
public:
    explicit Points(const std::vector<VERT_COMP>& components);
    Points(const Points&);
    Points(Points&&) noexcept;
    ~Points() = default;

    Points& operator=(const Points&);
    Points& operator=(Points&&) noexcept;

    void load(uint32_t vertex_count, const char* vertex_data, uint32_t vertex_size);
    void unload();

    uint32_t point_count() const { return vcnt; }

    std::vector<VERT_COMP> comps;
    uint32_t comp_size = 0;
    uint32_t vcnt = 0;
    std::unique_ptr<float[]> vbuf;
    bool loaded = false;
};

} // namespace vkkk
