#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "vk_ins/types.h"

namespace vkkk
{

class Lines {
public:
    Lines(const std::vector<VERT_COMP>& cs);
    Lines(const Lines&);
    Lines(Lines&&);
    ~Lines();

    Lines& operator=(const Lines&);
    Lines& operator=(Lines&&) noexcept;

    void load(const uint32_t v, const char* vdata, const uint32_t vsize);
    void load(const uint32_t v, const char* vdata, const uint32_t vsize,
        const uint32_t i, const char* idata, const uint32_t isize);
    void load(const glm::vec3& p0, const glm::vec3& p1);
    void unload();

    uint32_t line_count() const { return icnt / 2; }

    std::vector<VERT_COMP>      comps;
    uint32_t                    comp_size = 0;
    uint32_t                    vcnt = 0;
    std::unique_ptr<float[]>    vbuf;
    uint32_t                    icnt = 0;
    std::unique_ptr<uint32_t[]> ibuf;
    bool                        loaded = false;
};

} // namespace vkkk
