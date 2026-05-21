#include <stdexcept>
#include <cstring>
#include <utility>

#include <fmt/format.h>

#include "concepts/line.h"

namespace vkkk
{

Line::Line(const std::vector<VERT_COMP>& cs)
    : comps(cs)
{
    for (const auto& comp : comps)
        comp_size += comp_sizes[comp];
}

Line::Line(const Line& rhs)
    : comps(rhs.comps)
    , comp_size(rhs.comp_size)
    , vcnt(rhs.vcnt)
    , loaded(rhs.loaded)
{
    if (loaded) {
        vbuf = std::make_unique<float[]>(vcnt * comp_size);
        memcpy(vbuf.get(), rhs.vbuf.get(), vcnt * comp_size * sizeof(float));
    }
}

Line::Line(Line&& rhs)
    : comps(std::move(rhs.comps))
    , comp_size(rhs.comp_size)
    , vcnt(rhs.vcnt)
    , loaded(rhs.loaded)
{
    if (loaded) {
        vbuf = std::move(rhs.vbuf);
        rhs.loaded = false;
    }
}

Line& Line::operator=(const Line& rhs) {
    if (this == &rhs) {
        return *this;
    }

    comps = rhs.comps;
    comp_size = rhs.comp_size;
    vcnt = rhs.vcnt;
    loaded = rhs.loaded;

    if (loaded) {
        vbuf = std::make_unique<float[]>(vcnt * comp_size);
        memcpy(vbuf.get(), rhs.vbuf.get(), vcnt * comp_size * sizeof(float));
    } else {
        vbuf.reset();
    }

    return *this;
}

Line& Line::operator=(Line&& rhs) noexcept {
    if (this == &rhs) {
        return *this;
    }

    comps = std::move(rhs.comps);
    comp_size = rhs.comp_size;
    vcnt = rhs.vcnt;
    loaded = rhs.loaded;
    vbuf = std::move(rhs.vbuf);

    rhs.comp_size = 0;
    rhs.vcnt = 0;
    rhs.loaded = false;

    return *this;
}

Line::~Line() {}

void Line::load(const uint32_t v, const char* vdata, const uint32_t vsize) {
    const auto required_size = v * comp_size * sizeof(float);
    if (required_size != vsize) {
        throw std::length_error(fmt::format("Required line vbuf size : {}, actual : {}",
            required_size, vsize));
    }

    if (loaded)
        unload();

    vcnt = v;
    vbuf = std::make_unique<float[]>(vcnt * comp_size);
    memcpy(vbuf.get(), vdata, required_size);
    loaded = true;
}

void Line::load(const glm::vec3& p0, const glm::vec3& p1) {
    if (comps.empty() || comps[0] != VERTEX) {
        throw std::runtime_error("Line::load(p0, p1) requires VERTEX as first component");
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

void Line::unload() {
    vcnt = 0;
    vbuf.reset();
    loaded = false;
}

} // namespace vkkk
