#pragma once

#include <vector>

#include <vulkan/vulkan_raii.hpp>

namespace vkkk
{

class Context;

class CommandBuffers {
public:
    explicit CommandBuffers(Context* ctx);

    // Mirror command buffer handles currently owned by Context.
    void alloc();

    std::vector<vk::CommandBuffer> bufs;

    inline vk::CommandBuffer& operator[] (uint32_t idx) {
        return bufs[idx];
    }

    inline const vk::CommandBuffer& operator[] (uint32_t idx) const {
        return bufs[idx];
    }

private:
    Context* ctx_ = nullptr;
};

} // namespace vkkk
