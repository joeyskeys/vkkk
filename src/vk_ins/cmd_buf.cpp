#include "vk_ins/cmd_buf.h"

#include "vk_ins/context.hpp"

namespace vkkk
{

CommandBuffers::CommandBuffers(Context* ctx)
    : ctx_(ctx)
{
    alloc();
}

void CommandBuffers::alloc() {
    bufs.clear();
    if (ctx_ == nullptr) {
        return;
    }
    bufs.reserve(ctx_->command_buffers.size());
    for (const auto& cmd : ctx_->command_buffers) {
        bufs.push_back(*cmd);
    }
}

} // namespace vkkk
