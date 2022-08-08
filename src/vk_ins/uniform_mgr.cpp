#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

#include "uniform_mgr.h"

UniformMgr::UniformMgr(const VkDevice* device, uint32_t cnt)
    : device(dev)
    , swapchain_image_cnt(cnt)
{
    uniform_bufs.resize(swapchain_image_cnt);
    uniform_buf_mems.resize(swapchain_image_cnt);
}

UniformMgr::~UniformMgr()
{
    for (auto& bufs_per_swapchain : uniform_bufs) {
        for (auto& buf : bufs_per_swapchain) {
            vkDestroyBuffer(device, buf, nullptr);
        }
    }
    for (auto& mems_per_swapchain : uniform_buf_mems) {
        for (auto& mem : mems_per_swapchain) {
            vkFreeMemory(device, mem, nullptr);
        }
    }
    for (auto buf : ubo_bufs) {
        delete[] buf;
    }

    for (auto& img : uniform_imgs) {
        vkDestroyImage(device, img, nullptr);
    }
    for (auto& mem : uniform_img_mems) {
        vkFreeMemory(device, mem, nullptr);
    }
    for (auto buf : img_bufs) {
        delete[] buf;
    }
}

bool UniformMgr::add_uniform_buffer(uint32_t size) {
    std::vector<VkBuffer> bufs(swapchain_image_cnt);
    std::vector<VkDeviceMemory> mems(swapchain_image_cnt);

    for (int i = 0; i < swapchain_image_cnt; i++) {
        create_buffer(size, VK_BUFFER_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            bufs[i], mems[i]);
    }

    uniform_bufs.push_back(bufs);
    uniform_buf_mems.push_back(mems);

    void* buf = new char[size];
    ubo_bufs.push_back(buf);
}

bool UniformMgr::add_texture(const fs::path& path) {
    if (!fs::exists(path))
        return false;

    OIIO::ImageBuf oiio_buf(path.string().c_str());
    if (!oiio_buf.init_spec(oiio_buf.name(), 0, 0))
        return false;

    oiio_buf.read();
    int ch_ords[] = { 0, 1, 2, -1 };
    float ch_vals[] = { 0, 0, 0, 1.f };
    std::string ch_names[] = { "", "", "", "A" };
    OIIO::ImageBuf with_alpha_buf = OIIO::ImageBufAlgo::channels(oiio_buf, 4, ch_ords, ch_vals, ch_names);

    auto spec = with_alpha_buf.spec();
    int w = spec.width;
    int h = spec.height;
    // TODO : explicitly define pixel format
    VkDeviceSize img_size = w * h * 4;
    //std::vector<char> pixels;
    //pixels.resize(img_size);
    void* pixels = new char[img_size];
    with_alpha_buf.get_pixels(OIIO::ROI::All(), OIIO::TypeDesc::UINT8, pixels.data());

    VkBuffer staging_buf;
    VkDeviceMemory staging_buf_mem;
    create_buffer(img_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        staging_buf, staging_buf_mem);

    void* data;
    vkMapMemory(device, staging_buf_mem, 0, img_size, 0, &data);
        memcpy(data, pixels.data(), img_size);
    vkUnmapMemory(device, staging_buf_mem);
    img_bufs.push_back(pixels);

    VkImage tex_image;
    VkDeviceMemory tex_image_mem;
    create_image(w, h, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tex_image, tex_image_mem);

    transist_image_layout(tex_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copy_buffer_to_image(staging_buf, tex_image, w, h);
    transist_image_layout(tex_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(device, staging_buf, nullptr);
    vkFreeMemory(device, staging_buf_mem, nullptr);

    return true;
}

void UniformMgr::create_buffer(VkDeivceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties, VkBuffer& buffer,
    VkDeviceMemory& buffer_memory) {
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult res = vkCreateBuffer(device, &buffer_info, nullptr, &buffer);
    if (res != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device, buffer, &mem_requirements);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(mem_requirements.memoryTypeBits, properties);

    res = vkAllocateMemory(device, &alloc_info, nullptr, &buffer_memory);
    if (res != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, buffer_memory, 0);
}

uint32_t UniformMgr::find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) {
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if (type_filter & (1 << i) && (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type!");
}

void UniformMgr::create_image(uint32_t w, uint32_t h, VkFormat format,
    VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
    VkImage& image, VkDeviceMemory& image_memory)
{
    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = w;
    image_info.extent.height = h;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = format;
    image_info.tiling = tiling;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult res = vkCreateImage(device, &image_info, nullptr, &image);
    if (res != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(device, image, &mem_requirements);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(mem_requirements.memoryTypeBits, properties);

    res = vkAllocateMemory(device, &alloc_info, nullptr, &image_memory);
    if (res != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(device, image, image_memory, 0);
}

VkCommandBuffer UniformMgr::begin_single_time_commands() {
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = command_pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(device, &alloc_info, &command_buffer);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(command_buffer, &begin_info);

    return command_buffer;
}

void UniformMgr::end_single_time_commands(VkCommandBuffer cmd_buf) {
    vkEndCommandBuffer(cmd_buf);

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buf;

    vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue);

    vkFreeCommandBuffers(device, command_pool, 1, &cmd_buf);
}

void UniformMgr::transist_image_layout(VkImage image, VkFormat format,
    VkImageLayout old_layout, VkImageLayout new_layout)
{
    VkCommandBuffer cmd_buf = begin_single_time_commands();

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags src_stage;
    VkPipelineStageFlags dst_stage;

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(
        cmd_buf,
        src_stage, dst_stage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    end_single_time_commands(cmd_buf);
}