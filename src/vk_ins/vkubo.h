#pragma once

#include <vector>
#include <utility>

#include <vulkan/vulkan.h>

namespace vkkk
{

class VkWrappedInstance;

class UBO {
public:
    size_t                                  size;
    VkWrappedInstance*                      instance;
    std::unique_ptr<char[]>                 cpu_buf;
    std::vector<VkBuffer>                   gpu_bufs;
    std::vector<VkDeviceMemory>             memos;

    UBO(VkWrappedInstance* ins, size_t s);
    ~UBO();

    void update(uint32_t idx);
};

}