#include "shading/vkabstraction.h"
#include "shading/vktexture.h"

namespace vkkk
{

UBO::UBO(VkWrappedInstance* ins, size_t s)
    : instance(ins)
    , size(s)
{
    cpu_buf = std::make_unique<char[]>(size);

    auto cnt = ins->get_swapchain_cnt();
    gpu_bufs.resize(cnt);
    memos.resize(cnt);

    for (int i = 0; i < cnt; ++i)
        ins->create_buffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            gpu_bufs[i], memos[i]);
}

void UBO::update(uint32_t idx) {
    void* mapped_data;
    vkMapMemory(instance->get_device(), memos[idx], 0, size, 0, &mapped_data);
        memcpy(data, cpu_buf.get(), size);
    vkUnmapMemory(instance->get_device(), memos[idx]);
}

}