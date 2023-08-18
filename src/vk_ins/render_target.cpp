#include "vk_ins/render_target.h"
#include "vk_ins/vkabstraction.h"

namespace vkkk
{

RenderTarget::RenderTarget(VkWrappedInstance* i)
    : ins(i)
{
    auto& extent = ins->get_swapchain_extent();
    ins->create_vk_image(extent.width, extent.height, 1, ins->nsample, ins->get_swapchain_format(),
        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, memo);
    ins->create_imageview(image, ins->get_swapchain_format(), VK_IMAGE_ASPECT_COLOR_BIT);
}

void RenderTarget::free_gpu_resources() {
    vkDestroyImageView(ins->get_device(), view, nullptr);
    vkDestroyImage(ins->get_device(), image, nullptr);
    vkFreeMemory(ins->get_device(), memo, nullptr);
}

RenderTargetFromSwapchain::RenderTargetFromSwapchain(VkWrappedInstance* ins) {
    uint32_t cnt;
    vkGetSwapchainImagesKHR(ins->get_device(), ins->get_swapchain(), &cnt, nullptr);
    images.resize(cnt);
    vkGetSwapchainImagesKHR(isn->get_device(), ins->get_swapchain(), &cnt, images.data());
}

void RenderTargetFromSwapchain::free_gpu_resources() {
    // Nothing to be done
}

}