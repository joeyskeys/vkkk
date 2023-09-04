#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace vkkk
{

class VkWrappedInstance;

class RenderTargetDeprecated {
public:
    RenderTargetDeprecated(VkWrappedInstance*);

    void free_gpu_resources();

    VkWrappedInstance*          ins;
    VkImage                     image;
    VkDeviceMemory              memo;
    VkImageView                 view;
};

class RenderTargetFromSwapchainDeprecated {
public:
    RenderTargetFromSwapchainDeprecated(VkWrappedInstance*);

    void free_gpu_resources();

    std::vector<VkImage>        images;
    std::vector<VkImageView>    views;
};

}