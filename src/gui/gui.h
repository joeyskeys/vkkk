#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

struct ImDrawData;

namespace vkkk
{

class Context;

class ImGuiHud {
public:
    bool init(Context* ctx);
    void begin_frame();
    void build_default_hud(float fps, uint32_t drawable_count);
    void render(VkCommandBuffer cmd);
    void shutdown();

private:
    Context* ctx_ = nullptr;
    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
    bool initialized_ = false;
};

} // namespace vkkk