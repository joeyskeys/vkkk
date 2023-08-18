#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace vkkk
{

class VkWrappedInstance;

class RenderTarget {
    RenderTarget(VkWrappedInstance*);

    void free_gpu_resources();

    VkWrappedInstance*          ins;
    VkImage                     image;
    VkDeviceMemory              memo;
    VkImageView                 view;
};

class RenderTargetFromSwapchain {
public:
    RenderTargetFromSwapchain(VkWrappedInstance*);

    void free_gpu_resources();

    std::vector<VkImage>        images;
    std::vector<VkImageView>    views;
};

}