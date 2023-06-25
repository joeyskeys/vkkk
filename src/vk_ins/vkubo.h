#pragma once

#include <vector>
#include <utility>

#include <vulkan/vulkan.h>

namespace vkkk
{

class VkWrappedInstance;

class UBO {
public:
    VkWrappedInstance*                      instance;
    VkShaderStageFlagBits                   stage;
    std::string                             name;
    size_t                                  size;
    size_t                                  vecsize;
    uint32_t                                binding;
    std::unique_ptr<char[]>                 cpu_buf;
    std::vector<VkBuffer>                   gpu_bufs;
    std::vector<VkDeviceMemory>             memos;

    UBO(VkWrappedInstance*, const VkShaderStageFlagBits, uint32_t, size_t, size_t vs=1);
    ~UBO();
    UBO(UBO&& rhs);

    void update(uint32_t idx);
};

}