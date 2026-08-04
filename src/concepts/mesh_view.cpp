#include "concepts/mesh_view.hpp"

namespace vkkk
{

MeshView::MeshView(std::vector<VERT_COMP> cs, const float* vb, uint32_t v,
    const uint32_t* ib, uint32_t i, bool idx)
    : comps(std::move(cs))
    , indexed(idx)
    , comp_size(get_mesh_component_size(comps))
    , vcnt(v)
    , vbuf(vb)
    , icnt(i)
    , ibuf(ib)
{
}

MeshView::MeshView(const Mesh& mesh)
{
    if (!mesh.loaded)
        return;

    comps = mesh.comps;
    indexed = mesh.indexed;
    comp_size = mesh.comp_size;
    vcnt = mesh.vcnt;
    vbuf = mesh.vbuf;
    icnt = mesh.icnt;
    ibuf = mesh.ibuf;
}

bool MeshView::empty() const {
    if (vcnt == 0 || vbuf == nullptr)
        return true;
    if (indexed && (icnt == 0 || ibuf == nullptr))
        return true;
    return false;
}

uint32_t MeshView::vertex_float_count() const {
    return vcnt * comp_size;
}

uint32_t MeshView::vertex_bytes() const {
    return vertex_float_count() * static_cast<uint32_t>(sizeof(float));
}

uint32_t MeshView::index_count() const {
    return icnt * 3;
}

uint32_t MeshView::index_bytes() const {
    return index_count() * static_cast<uint32_t>(sizeof(uint32_t));
}

} // namespace vkkk
