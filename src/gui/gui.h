#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

struct ImDrawData;

namespace vkkk
{

class VkWrappedInstance;

class ImGuiHud {
public:
    bool init(VkWrappedInstance* ins);
    void begin_frame();
    void build_default_hud(float fps, uint32_t drawable_count);
    void render(VkCommandBuffer cmd);
    void shutdown();

private:
    VkWrappedInstance* ins_ = nullptr;
    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
    bool initialized_ = false;
};

} // namespace vkkk