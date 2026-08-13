#include <stdexcept>
#include <cstring>
#include <utility>

#include <fmt/format.h>

#include "concepts/line.h"

namespace vkkk
{

Lines::Lines(const std::vector<VERT_COMP>& cs)
    : comps(cs)
{
    for (const auto& comp : comps)
        comp_size += comp_sizes[comp];
}

Lines::Lines(const Lines& rhs)
    : comps(rhs.comps)
    , comp_size(rhs.comp_size)
    , vcnt(rhs.vcnt)
    , icnt(rhs.icnt)
    , loaded(rhs.loaded)
{
    if (loaded) {
        vbuf = std::make_unique<float[]>(vcnt * comp_size);
        memcpy(vbuf.get(), rhs.vbuf.get(), vcnt * comp_size * sizeof(float));
        ibuf = std::make_unique<uint32_t[]>(icnt);
        memcpy(ibuf.get(), rhs.ibuf.get(), icnt * sizeof(uint32_t));
    }
}

Lines::Lines(Lines&& rhs)
    : comps(std::move(rhs.comps))
    , comp_size(rhs.comp_size)
    , vcnt(rhs.vcnt)
    , icnt(rhs.icnt)
    , loaded(rhs.loaded)
{
    if (loaded) {
        vbuf = std::move(rhs.vbuf);
        ibuf = std::move(rhs.ibuf);
        rhs.loaded = false;
    }
}

Lines& Lines::operator=(const Lines& rhs) {
    if (this == &rhs) {
        return *this;
    }

    comps = rhs.comps;
    comp_size = rhs.comp_size;
    vcnt = rhs.vcnt;
    icnt = rhs.icnt;
    loaded = rhs.loaded;

    if (loaded) {
        vbuf = std::make_unique<float[]>(vcnt * comp_size);
        memcpy(vbuf.get(), rhs.vbuf.get(), vcnt * comp_size * sizeof(float));
        ibuf = std::make_unique<uint32_t[]>(icnt);
        memcpy(ibuf.get(), rhs.ibuf.get(), icnt * sizeof(uint32_t));
    } else {
        vbuf.reset();
        ibuf.reset();
    }

    return *this;
}

Lines& Lines::operator=(Lines&& rhs) noexcept {
    if (this == &rhs) {
        return *this;
    }

    comps = std::move(rhs.comps);
    comp_size = rhs.comp_size;
    vcnt = rhs.vcnt;
    icnt = rhs.icnt;
    loaded = rhs.loaded;
    vbuf = std::move(rhs.vbuf);
    ibuf = std::move(rhs.ibuf);

    rhs.comp_size = 0;
    rhs.vcnt = 0;
    rhs.icnt = 0;
    rhs.loaded = false;

    return *this;
}

Lines::~Lines() {}

void Lines::load(const uint32_t v, const char* vdata, const uint32_t vsize) {
    if (v % 2 != 0) {
        throw std::length_error("Lines vertex count must be even for line-list drawing");
    }
    const auto required_size = v * comp_size * sizeof(float);
    if (required_size != vsize) {
        throw std::length_error(fmt::format("Required line vbuf size : {}, actual : {}",
            required_size, vsize));
    }

    if (loaded)
        unload();

    std::vector<uint32_t> indices(v);
    for (uint32_t index = 0; index < v; ++index) {
        indices[index] = index;
    }
    load(v, vdata, vsize, v, reinterpret_cast<const char*>(indices.data()),
        static_cast<uint32_t>(indices.size() * sizeof(uint32_t)));
}

void Lines::load(const uint32_t v, const char* vdata, const uint32_t vsize,
    const uint32_t i, const char* idata, const uint32_t isize)
{
    if (v == 0 || i == 0 || i % 2 != 0) {
        throw std::length_error("Lines requires vertices and an even index count for line-list drawing");
    }
    if (vdata == nullptr || idata == nullptr) {
        throw std::invalid_argument("Lines vertex and index data must not be null");
    }
    const auto vertex_size = v * comp_size * sizeof(float);
    const auto index_size = i * sizeof(uint32_t);
    if (vertex_size != vsize || index_size != isize) {
        throw std::length_error("Lines vertex or index buffer size is invalid");
    }
    const auto indices = reinterpret_cast<const uint32_t*>(idata);
    for (uint32_t index = 0; index < i; ++index) {
        if (indices[index] >= v) {
            throw std::out_of_range("Lines index exceeds vertex count");
        }
    }

    if (loaded) {
        unload();
    }

    vcnt = v;
    vbuf = std::make_unique<float[]>(vcnt * comp_size);
    memcpy(vbuf.get(), vdata, vertex_size);
    icnt = i;
    ibuf = std::make_unique<uint32_t[]>(icnt);
    memcpy(ibuf.get(), idata, index_size);
    loaded = true;
}

void Lines::load(const glm::vec3& p0, const glm::vec3& p1) {
    if (comps.empty() || comps[0] != VERTEX) {
        throw std::runtime_error("Lines::load(p0, p1) requires VERTEX as first component");
    }

    std::vector<float> data;
    data.reserve(2 * comp_size);
    auto append_vertex = [&](const glm::vec3& pos) {
        for (const auto comp : comps) {
            switch (comp) {
                case VERTEX:
                    data.push_back(pos.x);
                    data.push_back(pos.y);
                    data.push_back(pos.z);
                    break;
                case NORMAL:
                    data.push_back(0.0f);
                    data.push_back(0.0f);
                    data.push_back(1.0f);
                    break;
                case UV:
                    data.push_back(0.0f);
                    data.push_back(0.0f);
                    break;
                case COLOR:
                    data.push_back(1.0f);
                    data.push_back(1.0f);
                    data.push_back(1.0f);
                    break;
            }
        }
    };

    append_vertex(p0);
    append_vertex(p1);

    load(2, reinterpret_cast<const char*>(data.data()),
        static_cast<uint32_t>(data.size() * sizeof(float)));
}

void Lines::unload() {
    vcnt = 0;
    vbuf.reset();
    icnt = 0;
    ibuf.reset();
    loaded = false;
}

} // namespace vkkk
