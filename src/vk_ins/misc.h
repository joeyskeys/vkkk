#pragma once

#include <utility>
#include <vulkan/vulkan.h>

namespace vkkk
{

void create_buffer(const VkDevice&, VkPhysicalDeviceMemoryProperties, VkDeviceSize,
    VkBufferUsageFlags, VkMemoryPropertyFlags, VkBuffer&, VkDeviceMemory&);

std::pair<VkImage, VkDeviceMemory> create_image(const VkDevice&, const uint32_t,
    const uint32_t, const VkFormat, const VkImageTiling, const VkImageUsageFlags,
    const VkMemoryPropertyFlags);

VkImageView create_imageview(const VkDevice&, const VkImage, const VkFormat,
    const VkImageAspectFlags);

VkSampler create_sampler();

}