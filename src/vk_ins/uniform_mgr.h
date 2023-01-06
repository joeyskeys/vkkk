#pragma once

#include <vector>
#include <filesystem>
#include <utility>

#include <vulkan/vulkan.h>

#include "vk_ins/vktexture.h"
#include "vk_ins/vkubo.h"

namespace fs = std::filesystem;

namespace vkkk
{

class VkWrappedInstance;

class UniformMgr {
public:
    UniformMgr(VkWrappedInstance* ins);
    virtual ~UniformMgr();

    bool add_buffer(const std::string& name, VkShaderStageFlagBits t,
        uint32_t size, uint32_t vecsize=1);
    bool add_texture(const std::string& name, VkShaderStageFlagBits t);
    void generate_writes();
    void set_dest_set(const uint32_t dst_set);

    std::vector<UBO>                    ubos;
    std::vector<Texture>                textures;
    std::vector<VkWriteDescriptorSet>   writes;

protected:
    VkWrappedInstance*                  instance;
    VkDevice device;
    VkPhysicalDeviceProperties          props;
    VkPhysicalDeviceMemoryProperties    mem_props;
    VkCommandPool command_pool;
    VkQueue graphic_queue;
    uint32_t swapchain_image_cnt;
};

}