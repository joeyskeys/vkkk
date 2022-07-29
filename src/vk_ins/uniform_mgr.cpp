#include "uniform_mgr.h"

UniformMgr::UniformMgr(const VkDevice* device, uint32_t cnt)
    : device(dev)
    , swapchain_image_cnt(cnt)
{
    uniform_bufs.resize(swapchain_image_cnt);
    uniform_buf_mems.resize(swapchain_image_cnt);
}

UniformMgr::~UniformMgr()
{
    for (auto& bufs_per_swapchain : uniform_bufs) {
        for (auto& buf : bufs_per_swapchain) {
            vkDestroyBuffer(device, buf, nullptr);
        }
    }
    for (auto& mems_per_swapchain : uniform_buf_mems) {
        for (auto& mem : mems_per_swapchain) {
            vkFreeMemory(device, mem, nullptr);
        }
    }
}