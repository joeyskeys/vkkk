#include <algorithm>
#include <cstring>
#include <ranges>
#include <vector>

#include "vk_ins/context.hpp"

namespace vkkk
{

bool Context::is_device_suitable(const vk::raii::PhysicalDevice& device, vk::raii::SurfaceKHR& surface,
    const std::vector<const char*>& required_extensions)
{
    const bool support_vk_1_3 = device.getProperties().apiVersion >= VK_API_VERSION_1_3;

    const auto queue_families = device.getQueueFamilyProperties();
    bool support_graphics = false;
    bool support_present = false;
    for (uint32_t i = 0; i < queue_families.size(); ++i) {
        if (queue_families[i].queueFlags & vk::QueueFlagBits::eGraphics) {
            support_graphics = true;
            if (device.getSurfaceSupportKHR(i, *surface)) {
                support_present = true;
                break;
            }
        }
    }

    const auto available_extensions = device.enumerateDeviceExtensionProperties();
    const bool supports_all_required_extensions = std::ranges::all_of(required_extensions, [&available_extensions](const auto& extension) {
        return std::ranges::any_of(available_extensions, [extension](const auto& available_extension) {
            return std::strcmp(available_extension.extensionName, extension) == 0;
        });
    });

    return support_vk_1_3 && support_graphics && support_present && supports_all_required_extensions;
}

uint32_t Context::choose_min_image_count(const vk::SurfaceCapabilitiesKHR& surface_capabilities) {
    uint32_t min_image_count = surface_capabilities.minImageCount + 1;
    if (surface_capabilities.maxImageCount > 0 && min_image_count > surface_capabilities.maxImageCount) {
        min_image_count = surface_capabilities.maxImageCount;
    }
    return min_image_count;
}

vk::SurfaceFormatKHR Context::choose_swap_surface_format(const std::vector<vk::SurfaceFormatKHR>& surface_formats) {
    const auto format_iter = std::ranges::find_if(surface_formats, [](const auto& surface_format) {
        return surface_format.format == vk::Format::eB8G8R8A8Srgb
            && surface_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
    });
    return format_iter != surface_formats.end() ? *format_iter : surface_formats.front();
}

vk::PresentModeKHR Context::choose_present_mode(const std::vector<vk::PresentModeKHR>& present_modes) {
    const bool has_mailbox = std::ranges::any_of(present_modes, [](const auto& present_mode) {
        return present_mode == vk::PresentModeKHR::eMailbox;
    });
    return has_mailbox ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo;
}

vk::Extent2D Context::choose_swap_extent(const vk::SurfaceCapabilitiesKHR& surface_capabilities, GLFWwindow* window) {
    if (surface_capabilities.currentExtent.width != UINT32_MAX) {
        return surface_capabilities.currentExtent;
    }
    int framebuffer_width = 0;
    int framebuffer_height = 0;
    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
    return vk::Extent2D{
        std::clamp<uint32_t>(static_cast<uint32_t>(framebuffer_width),
            surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width),
        std::clamp<uint32_t>(static_cast<uint32_t>(framebuffer_height),
            surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height)
    };
}

} // namespace vkkk
