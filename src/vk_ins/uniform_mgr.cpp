#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

#include "uniform_mgr.h"
#include "vk_ins/vkabstraction.h"
#include "vk_ins/vktexture.h"
#include "vk_ins/vkubo.h"

namespace vkkk
{

UniformMgr::UniformMgr(VkWrappedInstance* ins)
    : instance(ins)
    , graphic_queue(ins->get_graphic_queue())
    , swapchain_image_cnt(ins->get_swapchain_cnt())
{
    device = ins->get_device();
    //uniform_bufs.resize(swapchain_image_cnt);
    //uniform_buf_mems.resize(swapchain_image_cnt);
}

UniformMgr::~UniformMgr()
{}

bool UniformMgr::add_buffer(const std::string& name, uint32_t size) {
    auto ubo = UBO(instance, size);
    ubos.emplace_back(std::move(ubo));
    return true;
}

bool UniformMgr::add_texture(const fs::path& path) {
    auto tex = Texture(instance);
    if (!tex.load_image(path))
        return false;
    textures.emplace_back(std::move(tex));
    return true;
}

void UniformMgr::generate_writes() {

}

void UniformMgr::set_dest_set(const uint32_t dst_set) {

}

}