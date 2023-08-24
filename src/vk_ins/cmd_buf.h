#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace vkkk
{

class VkWrappedInstance;

class CommandBuffers {
public:
    CommandBuffers(VkWrappedInstance*);

    void alloc();

    std::vector<VkCommandBuffer> bufs;

    inline VkCommandBuffer& operator[] (uint32_t idx) {
        return bufs[idx];
    }

    inline const VkCommandBuffer& operator[] (uint32_t idx) const {
        return bufs[idx];
    }

private:
    VkWrappedInstance* ins;
};

}