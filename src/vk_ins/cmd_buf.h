#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace vkkk
{

class VkWrappedInstance;

class CommandBuffers {
public:
    CommandBuffers(VkWrappedInstance*);

    bool alloc();

    std::vector<VkCommandBuffer> bufs;

private:
    VkWrappedInstance* ins;
};

}