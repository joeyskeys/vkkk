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

private:
    VkWrappedInstance* ins;
};

}