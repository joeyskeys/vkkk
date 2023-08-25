#pragma once

#include <memory>
#include <vector>
#include <utility>

#include <vulkan/vulkan.h>

namespace vkkk
{

class VkWrappedInstance;

class UBODeprecated {
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

    UBODeprecated(VkWrappedInstance*, const VkShaderStageFlagBits, uint32_t, size_t, size_t vs=1);
    ~UBODeprecated();
    UBODeprecated(UBODeprecated&& rhs);
    UBODeprecated(const UBODeprecated& rhs) = delete;

    void free_gpu_resources();

    void update_descriptor();
    void update(uint32_t idx);

private:
    bool                                    loaded = false;
};

}