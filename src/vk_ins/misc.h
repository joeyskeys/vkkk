#pragma once

#include <vulkan/vulkan.h>

void create_buffer(const VkDevice&, VkDeviceSize, VkBufferUsageFlags,
    VkMemoryPropertyFlags, VkBuffer&, VkDeviceMemory&);