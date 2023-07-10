
#include "vk_ins/cmd_buf.h"
#include "vk_ins/vkabstraction.h"

namespace vkkk
{

CommandBuffers::CommandBuffers(VkWrappedInstance* i)
    : ins(i)
{
    bufs.resize(ins->get_swapchain_cnt());
}

void CommandBuffers::alloc() {
    // This design is weird...
    ins->alloc_commandbuffers(bufs);
}

}