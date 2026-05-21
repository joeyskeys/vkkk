#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "vk_ins/types.h"

namespace vkkk
{

class Line {
public:
    Line(const std::vector<VERT_COMP>& cs);
    Line(const Line&);
    Line(Line&&);
    ~Line();

    Line& operator=(const Line&);
    Line& operator=(Line&&) noexcept;

    void load(const uint32_t v, const char* vdata, const uint32_t vsize);
    void load(const glm::vec3& p0, const glm::vec3& p1);
    void unload();

    std::vector<VERT_COMP>      comps;
    uint32_t                    comp_size = 0;
    uint32_t                    vcnt = 0;
    std::unique_ptr<float[]>    vbuf;
    bool                        loaded = false;
};

} // namespace vkkk
