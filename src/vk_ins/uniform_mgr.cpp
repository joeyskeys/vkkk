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
    , ubos()
    , textures()
{
    device = ins->get_device();
    //uniform_bufs.resize(swapchain_image_cnt);
    //uniform_buf_mems.resize(swapchain_image_cnt);
}

UniformMgr::UniformMgr(UniformMgr&& rhs)
    : instance(rhs.instance)
    , device(rhs.device)
    , props(rhs.props)
    , mem_props(rhs.mem_props)
    , command_pool(rhs.command_pool)
    , graphic_queue(rhs.graphic_queue)
    , swapchain_image_cnt(rhs.swapchain_image_cnt)
    , ubos(std::move(rhs.ubos))
    , textures(std::move(rhs.textures))
    , writes(std::move(rhs.writes))
{}

UniformMgr::~UniformMgr()
{}

bool UniformMgr::add_buffer(const std::string& name, VkShaderStageFlagBits t,
    uint32_t binding, uint32_t size, uint32_t vecsize)
{
    auto ubo = UBO(instance, t, binding, size, vecsize);
    ubo.name = name;
    //ubos.emplace_back(std::move(ubo));
    ubos.emplace(name, std::move(ubo));
    return true;
}

bool UniformMgr::add_texture(const std::string& name, VkShaderStageFlagBits t,
    uint32_t binding, const std::string& path) {
    auto tex = Texture(instance, name, t, binding);
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