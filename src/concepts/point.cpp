#include "concepts/point.h"

#include <cstring>
#include <stdexcept>
#include <utility>

namespace vkkk
{

Points::Points(const std::vector<VERT_COMP>& components)
    : comps(components)
{
    for (const auto component : comps) {
        comp_size += comp_sizes[component];
    }
}

Points::Points(const Points& other)
    : comps(other.comps)
    , comp_size(other.comp_size)
    , vcnt(other.vcnt)
    , loaded(other.loaded)
{
    if (loaded) {
        vbuf = std::make_unique<float[]>(vcnt * comp_size);
        std::memcpy(vbuf.get(), other.vbuf.get(), vcnt * comp_size * sizeof(float));
    }
}

Points::Points(Points&& other) noexcept
    : comps(std::move(other.comps))
    , comp_size(other.comp_size)
    , vcnt(other.vcnt)
    , vbuf(std::move(other.vbuf))
    , loaded(other.loaded)
{
    other.comp_size = 0;
    other.vcnt = 0;
    other.loaded = false;
}

Points& Points::operator=(const Points& other) {
    if (this == &other) {
        return *this;
    }
    Points copy(other);
    *this = std::move(copy);
    return *this;
}

Points& Points::operator=(Points&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    comps = std::move(other.comps);
    comp_size = other.comp_size;
    vcnt = other.vcnt;
    vbuf = std::move(other.vbuf);
    loaded = other.loaded;

    other.comp_size = 0;
    other.vcnt = 0;
    other.loaded = false;
    return *this;
}

void Points::load(uint32_t vertex_count, const char* vertex_data, uint32_t vertex_size) {
    if (vertex_count == 0 || vertex_data == nullptr) {
        throw std::invalid_argument("Points requires non-empty vertex data");
    }

    const uint32_t required_vertex_size = vertex_count * comp_size * sizeof(float);
    if (vertex_size != required_vertex_size) {
        throw std::length_error("Point-list vertex buffer size is invalid");
    }

    vcnt = vertex_count;
    vbuf = std::make_unique<float[]>(vcnt * comp_size);
    std::memcpy(vbuf.get(), vertex_data, required_vertex_size);
    loaded = true;
}

void Points::unload() {
    vcnt = 0;
    vbuf.reset();
    loaded = false;
}

} // namespace vkkk
