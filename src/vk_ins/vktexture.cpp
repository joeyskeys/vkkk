#include <fmt/format.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

#include "vk_ins/vkabstraction.h"
#include "vk_ins/vktexture.h"

namespace vkkk
{

Texture::Texture(VkWrappedInstance* ins, const std::string& n, VkShaderStageFlagBits t, uint32_t b)
    : instance(ins)
    , name(n)
    , stage(t)
    , binding(b)
{}

Texture::~Texture() {
    if (loaded) {
        destroy();
        loaded = false;
    }
}

Texture::Texture(Texture&& t)
    : instance(t.instance)
    , name(std::move(t.name))
    , stage(t.stage)
    , vecsize(t.vecsize)
    , binding(t.binding)
    , image(t.image)
    , image_layout(t.image_layout)
    , memory(t.memory)
    , view(t.view)
    , width(t.width)
    , height(t.height)
    , mipmap_lv(t.mipmap_lv)
    , layout_cnt(t.layout_cnt)
    , descriptor(t.descriptor)
    , sampler(t.sampler)
    , loaded(t.loaded)
{
    t.loaded = false;
}

void Texture::update_descriptor() {
    descriptor.sampler = sampler;
    descriptor.imageView = view;
    descriptor.imageLayout = image_layout;
}

void Texture::destroy() {
    vkDestroyImageView(instance->get_device(), view, nullptr);
    vkDestroyImage(instance->get_device(), image, nullptr);
    if (sampler)
        vkDestroySampler(instance->get_device(), sampler, nullptr);

    vkFreeMemory(instance->get_device(), memory, nullptr);
}

bool Texture::load_image(const fs::path& path) {
    fs::path abs_path = path;
    if (path.is_relative())
        abs_path = fs::absolute(path);
    if (!fs::exists(abs_path))
        throw std::runtime_error(fmt::format("texture does not exists .. {}", path.string()));

    OIIO::ImageBuf oiio_buf(abs_path.string().c_str());
    if (!oiio_buf.init_spec(oiio_buf.name(), 0, 0))
        throw std::runtime_error(fmt::format("texture init spec failed : {}", abs_path.string()));

    oiio_buf.read();
    oiio_buf = OIIO::ImageBufAlgo::flip(oiio_buf);

    int ch_ords[] = {0, 1, 2, -1};
    float ch_vals[] = {0, 0, 0, 1.f};
    std::string ch_names[] = {"R", "G", "B", "A"};
    OIIO::ImageBuf with_alpha_buf = OIIO::ImageBufAlgo::channels(oiio_buf, 4, ch_ords, ch_vals, ch_names);

    auto spec = with_alpha_buf.spec();
    width = spec.width;
    height = spec.height;
    std::cout << "tex w : " << width << ", h : " << height << std::endl;
    VkDeviceSize image_size = width * height * sizeof(Pixel);

    std::vector<Pixel> pixels;
    pixels.resize(width * height);
    with_alpha_buf.get_pixels(OIIO::ROI::All(), OIIO::TypeDesc::UINT8, pixels.data());

    VkBuffer staging_buf;
    VkDeviceMemory staging_buf_memo;
    instance->create_buffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        staging_buf, staging_buf_memo);

    void* data;
    vkMapMemory(instance->get_device(), staging_buf_memo, 0, image_size, 0, &data);
        memcpy(data, pixels.data(), static_cast<size_t>(image_size));
    vkUnmapMemory(instance->get_device(), staging_buf_memo);

    instance->create_vk_image(width, height, 1, VK_SAMPLE_COUNT_1_BIT,
        VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, memory);

    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;

    std::vector<VkBufferImageCopy> regions;
    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};
    regions.push_back(region);

    instance->transition_image_layout(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, range);
        instance->copy_buffer_to_image(staging_buf, image, regions);
    instance->transition_image_layout(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, range);

    vkDestroyBuffer(instance->get_device(), staging_buf, nullptr);
    vkFreeMemory(instance->get_device(), staging_buf_memo, nullptr);

    // Create imageview
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = VK_FORMAT_R8G8B8A8_SRGB;
    view_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    view_info.image = image;
    if (vkCreateImageView(instance->get_device(), &view_info, nullptr, &view) != VK_SUCCESS)
        throw std::runtime_error(fmt::format("failed to create imageview for texture {}", name));

    // Create sampler
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.mipLodBias = 0.f;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.minLod = 0.f;
    sampler_info.maxLod = 0.f;
    sampler_info.maxAnisotropy = 1.f;
    sampler_info.anisotropyEnable = false;
    sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    if (vkCreateSampler(instance->get_device(), &sampler_info, nullptr, &sampler) != VK_SUCCESS)
        throw std::runtime_error(fmt::format("failed to create sampler for texture {}", name));

    update_descriptor();

    loaded = true;
    return true;
}

bool Texture::load_cubemap(const fs::path& path) {
    fs::path abs_path = path;
    if (path.is_relative())
        abs_path = fs::absolute(path);
    if (!fs::exists(abs_path))
        throw std::runtime_error(fmt::format("texture does not exists .. {}", path.string()));

    OIIO::ImageBuf oiio_buf(abs_path.string().c_str());
    if (!oiio_buf.init_spec(oiio_buf.name(), 0, 0))
        throw std::runtime_error(fmt::format("texture init spec failed : {}", abs_path.string()));

    oiio_buf.read();
    auto spec = oiio_buf.spec();
    auto size = spec.width / 4;

    auto top_buf = OIIO::ImageBufAlgo::cut(oiio_buf, OIIO::ROI(size, size * 2, 0, size));
    auto bottom_buf = OIIO::ImageBufAlgo::cut(oiio_buf, OIIO::ROI(size, size * 2, size * 2, size * 3));
    auto left_buf = OIIO::ImageBufAlgo::cut(oiio_buf, OIIO::ROI(0, size, size, size * 2));
    auto right_buf = OIIO::ImageBufAlgo::cut(oiio_buf, OIIO::ROI(size * 2, size * 3, size, size * 2));
    auto front_buf = OIIO::ImageBufAlgo::cut(oiio_buf, OIIO::ROI(size, size * 2, size, size * 2));
    auto back_buf = OIIO::ImageBufAlgo::cut(oiio_buf, OIIO::ROI(size * 3, size * 4, size, size * 2));
    std::array<OIIO::ImageBuf, 6> bufs = {right_buf, left_buf, top_buf, bottom_buf, front_buf, back_buf};

    std::vector<Pixel> pixel_pool;
    pixel_pool.reserve(size * size * 6);
    VkDeviceSize image_size = size * size * sizeof(Pixel) * 6;

    for (auto& buf : bufs) {
        /*
        // Interestingly, cubemap doesn't need to do any flipping
        if (reorient_funcs[i])
            buf = reorient_funcs[i](buf, OIIO::ROI{}, 0);
        */
        int ch_ords[] = {0, 1, 2, -1};
        float ch_vals[] = {0, 0, 0, 1.f};
        std::string ch_names[] = {"R", "G", "B", "A"};
        OIIO::ImageBuf with_alpha_buf = OIIO::ImageBufAlgo::channels(buf, 4, ch_ords, ch_vals, ch_names);

        std::vector<Pixel> pixels;
        pixels.resize(size * size);
        with_alpha_buf.get_pixels(OIIO::ROI::All(), OIIO::TypeDesc::UINT8, pixels.data());
        pixel_pool.insert(pixel_pool.end(),
            std::make_move_iterator(pixels.begin()), std::make_move_iterator(pixels.end()));
    }

    VkBuffer staging_buf;
    VkDeviceMemory staging_buf_memo;
    instance->create_buffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        staging_buf, staging_buf_memo);
    
    void* data;
    vkMapMemory(instance->get_device(), staging_buf_memo, 0, image_size, 0, &data);
        memcpy(data, pixel_pool.data(), static_cast<size_t>(image_size));
    vkUnmapMemory(instance->get_device(), staging_buf_memo);

    instance->create_vk_image(size, size, 6, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, memory);

    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 6;

    size_t offset = 0;
    std::vector<VkBufferImageCopy> regions;
    for (int i = 0; i < 6; ++i) {
        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = i;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {size, size, 1};
        region.bufferOffset = offset;
        offset += size * size * sizeof(Pixel);
        regions.push_back(region);
    }

    instance->transition_image_layout(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, range);
        instance->copy_buffer_to_image(staging_buf, image, regions);
    instance->transition_image_layout(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, range);

    vkDestroyBuffer(instance->get_device(), staging_buf, nullptr);
    vkFreeMemory(instance->get_device(), staging_buf_memo, nullptr);

    // imageviews
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    view_info.format = VK_FORMAT_R8G8B8A8_SRGB;
    view_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6};
    view_info.image = image;
    if (vkCreateImageView(instance->get_device(), &view_info, nullptr, &view) != VK_SUCCESS)
        throw std::runtime_error(fmt::format("failed to create imageview for cubemap"));

    // sampler
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.mipLodBias = 0.f;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.minLod = 0.f;
    sampler_info.maxLod = 0.f;
    sampler_info.maxAnisotropy = 1.f;
    sampler_info.anisotropyEnable = false;
    sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    if (vkCreateSampler(instance->get_device(), &sampler_info, nullptr, &sampler) != VK_SUCCESS)
        throw std::runtime_error(fmt::format("failed to create sampler for texture {}", name));

    update_descriptor();

    vecsize = 6;
    loaded = true;
    return true;
}

}