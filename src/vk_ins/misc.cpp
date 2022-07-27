#include "misc.h"

#include <utility>

namespace vkkk
{

void create_buffer(const VkDevice& device, VkDeviceSize size,
    VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buf, 
    VkDeviceMemory& buf_memo)
{
    buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_info.size = size;
    buf_info.usage = usage;
    buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &buf_info, nullptr, &buf) != VK_SUCCESS)
        throw std::runtime_error("failed to create buffer!");

    VkMemoryRequirements memo_req;
    vkGetBufferMemoryRequirements(device, buf, &memo_req);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = memo_req.size;
    alloc_info.memoryTypeIndex = find_memory_type(memo_req.memoryTypeBits, props);

    if (vkAllocateMemory(device, &alloc_info, nullptr, &buf_memo) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate buffer memory!");

    vkBindBufferMemory(device, buf, buf_memo, 0);
}

std::pair<VkImage, VkDeviceMemory> create_image(const VkDevice& device,
    const uint32_t w, const uint32_t h, const VkFormat format, const VkImageTiling tiling,
    const VkImageUsageFlags usage, const VkMemoryPropertyFlags properties)
{
    VkImageCreateInfo img_info{};
    img_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    img_info.imageType = VK_IMAGE_TYPE_2D;
    img_info.extent.width = w;
    img_info.extent.height = h;
    img_info.extent.depth = 1;
    img_info.mipLevels= 1;
    img_info.arrayLayers = 1;
    img_info.format = format;
    img_info.tiling = tiling;
    img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    img_info.usage = usage;
    img_info.samples = VK_SAMPLE_COUNT_1_BIT;
    img_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkImage img;
    VkMemoryDevice memo;
    if (vkCreateImage(device, &img_info, nullptr, &img) != VK_SUCCESS)
        throw std::runtime_error("Failed to create image");

    VkMemoryRequirements mem_reqs{};
    vkGetImageMemoryRequirements(device, img, &mem_reqs);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = find_memory_type(mem_reqs[0].memoryTypeBits, properties);

    if (vkAllocateMemory(device, &alloc_info, nullptr, &memo) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate image memory");

    vkBindImageMemory(device, img, memo, 0);
    return std::make_pair(img, memo);
}

VkImageView create_imageview(const VkDevice& device, const VkImage img,
    const VkFormat fmt, const VkImageAspectFlags aspect_flags)
{
    VkImageViewCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.image = img;
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.format = fmt;

    info.subresourceRange.aspectMask = aspect_flags;
    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = 1;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = 1;

    VkImageView view;
    if (vkCreateImageView(device, &info, nullptr, &view) != VK_SUCCESS)
        throw std::runtime_error("Failed to create imageview");
    
    return view;
}

VkSampler create_sampler(const VkPhysicalDeviceProperties props) {
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.anisotropyEnable = VK_TRUE;

    //VkPhysicalDeviceProperties props{};
    //vkGetPhysicalDeviceProperties(physical_device, &props);
    sampler_info.maxAnisotropy = props.limits.maxSamplerAnisotropy;
    
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.mipLodBias = 0.f;
    sampler_info.minLod = 0.f;
    sampler_info.maxLod = 0.f;

    VkSampler sampler;
    if (vkCreateSampler(device, &sampler_info, nullptr, &sampler) != VK_SUCCESS)
        throw std::runtime_error("failed to create texture sampler");

    return sampler;
}

}