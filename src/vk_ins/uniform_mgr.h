#pragma once

#include <vector>
#include <filesystem>
#include <utility>

#include <vulkan/vulkan.h>

namespace fs = std::filesystem;

namespace vkkk
{

class UniformMgr {
public:
    UniformMgr(const VkDevice dev, const VkQueue graph_q, uint32_t cnt);
    virtual ~UniformMgr();

    bool add_buffer(const std::string& name, uint32_t size);
    bool add_texture(const fs::path& path);

protected:
    VkDevice device;
    VkPhysicalDeviceMemoryProperties mem_props;
    VkCommandPool command_pool;
    VkQueue graphic_queue;
    uint32_t swapchain_image_cnt;

    std::unordered_map<std::string, std::pair<uint32_t, char*>> ubo_bufs;

    // 2D array of uniform buffers with following structure
    // [swapchain 1 uniform buffers : [buf1] [buf2] [buf3]]
    // [swapchain 2 uniform buffers : [buf1] [buf2] [buf3]]
    std::vector<std::vector<VkBuffer>>  uniform_bufs;
    std::vector<std::vector<VkDeviceMemory>> uniform_buf_mems;

    std::vector<void*>                  img_bufs;
    std::vector<VkImage>                uniform_imgs;
    std::vector<VkDeviceMemory>         uniform_img_mems;
    std::vector<VkImageView>            uniform_img_views;

    void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties, VkBuffer& buffer,
        VkDeviceMemory& buffer_memory);

    uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) const;

    void create_image(uint32_t width, uint32_t height, VkFormat format,
        VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
        VkImage& image, VkDeviceMemory& image_memory);

    VkCommandBuffer begin_single_time_commands();

    void end_single_time_commands(VkCommandBuffer cmd_buf);

    void transist_image_layout(VkImage image, VkFormat format, VkImageLayout old_layout,
        VkImageLayout new_layout);

    void copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t w, uint32_t h);
};

}