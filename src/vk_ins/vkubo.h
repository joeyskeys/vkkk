#pragma once

#include <memory>
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
    std::vector<VkDescriptorBufferInfo>     descriptors;

    UBO(VkWrappedInstance*, const VkShaderStageFlagBits, uint32_t, size_t, size_t vs=1);
    ~UBO();
    UBO(UBO&& rhs);
    UBO(const UBO& rhs) = delete;

    void free_gpu_resources();

    void update_descriptor();
    void update(uint32_t idx);

private:
    bool                                    loaded = false;
};

}