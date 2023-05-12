#include "vk_ins/vkabstraction.h"
#include "vk_ins/vktexture.h"

namespace vkkk
{

UBO::UBO(VkWrappedInstance* ins, const VkShaderStageFlagBits t, size_t s, size_t vs)
    : instance(ins)
    , stage(t)
    , size(s)
    , vecsize(vs)
{
    cpu_buf = std::make_unique<char[]>(size * vecsize);

    auto cnt = ins->get_swapchain_cnt();
    gpu_bufs.resize(cnt);
    memos.resize(cnt);

    for (int i = 0; i < cnt; ++i)
        ins->create_buffer(size * vecsize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            gpu_bufs[i], memos[i]);
}

UBO::~UBO() {
    for (auto& gpu_buf : gpu_bufs)
        vkDestroyBuffer(instance->get_device(), gpu_buf, nullptr);
    for (auto& memo : memos)
        vkFreeMemory(instance->get_device(), memo, nullptr);
}

UBO::UBO(UBO&& rhs)
    : instance(rhs.instance)
    , stage(rhs.stage)
    , size(rhs.size)
    , vecsize(rhs.vecsize)
    , cpu_buf(std::move(rhs.cpu_buf))
    , gpu_bufs(std::move(rhs.gpu_bufs))
    , memos(std::move(rhs.memos))
{}

void UBO::update(uint32_t idx) {
    void* mapped_data;
    vkMapMemory(instance->get_device(), memos[idx], 0, size, 0, &mapped_data);
        memcpy(mapped_data, cpu_buf.get(), size);
    vkUnmapMemory(instance->get_device(), memos[idx]);
}

}