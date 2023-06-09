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

bool UniformMgr::add_buffer(const std::string& name, VkShaderStageFlagBits t,
    uint32_t size, uint32_t vecsize)
{
    auto ubo = UBO(instance, t, size, vecsize);
    ubo.name = name;
    //ubos.emplace_back(std::move(ubo));
    ubos.emplace(name, std::move(ubo));
    return true;
}

bool UniformMgr::add_texture(const std::string& name, VkShaderStageFlagBits t, const std::string& path) {
    auto tex = Texture(instance, name, t);
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