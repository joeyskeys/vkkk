#pragma once

#include <vector>
#include <filesystem>
#include <unordered_map>
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
        uint32_t binding, uint32_t size, uint32_t vecsize=1);
    bool add_texture(const std::string& name, VkShaderStageFlagBits t,
        uint32_t binding, const std::string& path);
    void generate_writes();
    void set_dest_set(const uint32_t dst_set);
    
    inline UBO* find_ubo(const std::string& name) {
        auto dest = ubos.find(name);
        if (dest == ubos.end())
            return nullptr;
        return &(dest->second);
    }

    inline void update_ubos(uint32_t idx) {
        for (auto& [ubo_name, ubo] : ubos)
            ubo.update(idx);
    }

    std::unordered_map<std::string, UBO>    ubos;
    std::vector<Texture>                    textures;
    std::vector<VkWriteDescriptorSet>       writes;

protected:
    VkWrappedInstance*                      instance;
    VkDevice                                device;
    VkPhysicalDeviceProperties              props;
    VkPhysicalDeviceMemoryProperties        mem_props;
    VkCommandPool                           command_pool;
    VkQueue                                 graphic_queue;
    uint32_t                                swapchain_image_cnt;
};

}