
#include "vktexture.h"

namespace vkkk
{

void Texture::update_descriptor() {
    descriptor.sampler = sampler;
    descriptor.imageView = view;
    descriptor.imageLayout = image_layout;
}

void Texture::destroy() {
    vkDestroyImageView(instance->get_device(), view, nullptr);
    vkDestroyImage(instance->get_device(), image, nullptr);
    if (sampler)
        vkDesctroySampler(instance->get_device(), sampler, nullptr);

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

    int ch_ords[] = {0, 1, 2, -1};
    float ch_vals[] = {0, 0, 0, 1.f};
    std::string ch_names[] = {"", "", "", "A"};
    OIIO::ImageBuf with_alpha_buf = OIIO::ImageBufAlgo::channels(oiio_buf, 4, ch_ords, ch_vals, ch_names);

    auto spec = with_alpha_buf.spec();
    int w = spec.width;
    int h = spec.height;
    std::cout << "tex w : " << w << ", h : " << h << std::endl;
    VkDeviceSize image_size = w * h * sizeof(Pixel);

    std::vector<Pixel> pixels;
    pixels.resize(w * h);
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

    instance->create_vk_image(w, h, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tex_img, tex_img_memo);

    instance->transition_image_layout(tex_img, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        instance->copy_buffer_to_image(staging_buf, tex_img, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
    instance->transition_image_layout(tex_img, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(instance->get_device(), staging_buf, nullptr);
    vkFreeMemory(instance->get_device(), staging_buf_memo, nullptr);

    return true;
}

}