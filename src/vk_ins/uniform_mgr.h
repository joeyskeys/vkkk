#pragma once

#include <vector>
#include <filesystem>

#include <vulkan/vulkan.h>

namespace fs = std::filesystem;

namespace vkkk
{

class UniformMgr {
public:
    UniformMgr(const VkDevice* dev, uint32_t cnt);
    virtual ~UniformMgr();

    bool add_uniform_buffer(uint32_t size);
    bool add_texture(const fs::path& path);

protected:
    VkDevice *device;
    uint32_t swapchain_image_cnt;

    void*                               bufs;

    // 2D array of uniform buffers with following structure
    // [swapchain 1 uniform buffers : [buf1] [buf2] [buf3]]
    // [swapchain 2 uniform buffers : [buf1] [buf2] [buf3]]
    std::vector<std::vector<VkBuffer>>  uniform_bufs;
    std::vector<std::vector<VkDeviceMemory>> uniform_buf_mems;

    std::vector<void*>                  img_bufs;
    std::vector<VkImage>                uniform_imgs;
    std::vector<VkDeviceMemory>         uniform_img_mems;
};

}