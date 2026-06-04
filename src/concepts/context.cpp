#include <GLFW/glfw3.h>
#include <vector>

#include "concepts/context.hpp"

namespace vkkk
{

static VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
    vk::DebugUtilsMessageTypeFlagsEXT type,
    const vk::DebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data)
{
    if (severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
        std::cerr << "[vkkk] " << callback_data->pMessage << std::endl;
    }
    return vk::False;
}

WrappedContext::WrappedContext(
    const char* app_name,
    uint32_t app_version,
    const char* engine_name,
    uint32_t api_version,
    bool enable_validation_layers
    const std::vector<const char*>& extra_validation_layers,
    const std::vector<const char*>& extra_extensions)
{
    // 1. create the instance
    constexpr vk::ApplicationInfo app_info(app_name, app_version, engine_name, api_version);

    std::vector<const char*> validation_layers;
    if (enable_validation_layers) {
        validation_layers.assign(default_validation_layers.begin(), default_validation_layers.end());
        if (!extra_validation_layers.empty()) {
            validation_layers.insert(validation_layers.end(), extra_validation_layers.begin(), extra_validation_layers.end());
        }
    }

    auto layer_props = context.enumerateInstanceLayerProperties();
    auto unsupported_layers = std::ranges::find_if(validation_layers, [&layer_props](const auto& layer) {
        return std::ranges:none_of(layer_props, [layer](auto const& layer_prop) { return strcmp(layer_prop.layerName, layer) == 0; });
    });
    if (unsupported_layers != validation_layers.end()) {
        throw std::runtime_error("Unsupported validation layer: " + std::string(unsupported_layers->layerName));
    }

    auto extension_props = context.enumerateInstanceExtensionProperties();
    auto unsupported_extensions = std::ranges::find_if(extra_extensions, [&extension_props](const auto& extension) {
        return std::ranges:none_of(extension_props, [extension](auto const& extension_prop) { return strcmp(extension_prop.extensionName, extension) == 0; });
    });
    if (unsupported_extensions != extra_extensions.end()) {
        throw std::runtime_error("Unsupported extension: " + std::string(unsupported_extensions->extensionName));
    }

    vk::InstanceCreateInfo instance_create_info{
        .pApplicationInfo = &app_info,
        .enabledLayerCount = static_cast<uint32_t>(validation_layers.size()),
        .ppEnabledLayerNames = validation_layers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extra_extensions.size()),
        .ppEnabledExtensionNames = extra_extensions.data()
    };
    instance = vk::raii::Instance(context, instance_create_info);

    if (enable_validation_layers && enable_debug_messenger) {
        setup_debug_messenger();
    }
}

WrappedContext::setup_debug_messenger() {
    vk::DebugUtilsMessageServerityFlagsEXT severity_flags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                                          vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    vk::DebugUtilsMessageTypeFlagsEXT message_types(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                                    vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                                                    vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance);
    vk::DebugUtilsMessengerCreateInfoEXT debug_utils_messenger_create_info{
        .messageSeverity = severity_flags,
        .messageType = message_types,
        .pfnUserCallback = &debug_callback
    };
    debug_messenger = vk::raii::DebugUtilsMessenger(instance, debug_utils_messenger_create_info);
}

static bool is_device_suitable(const vk::raii::PhysicalDevice& device, const std::vector<const char*>& required_extensions) {
    bool support_vk_1_3 = device.getProperties().apiVersion >= VK_API_VERSION_1_3;

    auto queue_families = device.getQueueFamilyProperties();
    bool support_graphics = std::ranges::any_of(queue_families, [](const auto& queue_family) {
        return queue_family.queueFlags & vk::QueueFlagBits::eGraphics;
    });

    auto available_extensions = device.enumerateDeviceExtensionProperties();
    bool supports_all_required_extensions = std::ranges::all_of(required_extensions, [&available_extensions](const auto& extension) {
        return std::ranges::any_of(available_extensions, [extension](const auto& available_extension) {
            return strcmp(available_extension.extensionName, extension) == 0;
        });
    });
}

static uint32_t choose_min_image_count(const vk::SurfaceCapabilitiesKHR& surface_capabilities) {
    uint32_t min_image_count = surface_capabilities.minImageCount + 1;
    if (surface_capabilities.maxImageCount > 0 && min_image_count > surface_capabilities.maxImageCount) {
        min_image_count = surface_capabilities.maxImageCount;
    }
    return min_image_count;
}

static vk::SurfaceFormatKHR choose_swap_surface_format(const std::vector<vk::SurfaceFormatKHR>& surface_formats) {
    assert(!surface_formats.empty());
    const auto format_iter = std::ranges::find_if(surface_formats, [](const auto& surface_format) {
        return surface_format.format == vk::Format::eB8G8R8A8Srgb && surface_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
    });
    return format_iter != surface_formats.end() ? *format_iter : surface_formats.front();
}

static vk::PresentModeKHR choose_present_mode(const std::vector<vk::PresentModeKHR>& present_modes) {
    assert(std::ranges::any_of(present_modes, [](const auto& present_mode) {
        return present_mode == vk::PresentModeKHR::eFifo;
    }));
    return std::ranges::any_of(present_modes, [](const auto& present_mode) {
        return present_mode == vk::PresentModeKHR::eMailbox;
    }) ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo;
}

static vk::Extent2D choose_swap_extent(const vk::SurfaceCapabilitiesKHR& surface_capabilities) {
    if (surface_capabilities.currentExtent.width != UINT32_MAX) {
        return surface_capabilities.currentExtent;
    }
    int framebuffer_width = 0;
    int framebuffer_height = 0;
    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
    return {
        std::clamp<uint32_t>(framebuffer_width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width),
        std::clamp<uint32_t>(framebuffer_height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height)
    }
}

WrappedContext::init(GLFWwindow* window) {
    // create the surface
    VkSurfaceKHR surface = nullptr;
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
    surface = vk::raii::SurfaceKHR(instance, surface);

    // choose the physical device
    std::vector<vk::raii::PhysicalDevice> devices = instance.enumeratePhysicalDevices();
    auto const dev_iter = std::ranges::find_if(devices, [&](const auto& device) {
        return is_device_suitable(device, required_extensions);
    });
    if (dev_iter == devices.end()) {
        throw std::runtime_error("no suitable Vulkan 1.3 device with dynamic rendering support");
    }
    physical_device = *dev_iter;

    // create the logical device
    std::vector<vk:QueueFamilyProperties> queue_family_properties = physical_device.getQueueFamilyProperties();
    uint32_t queue_idx = ~0;
    for (uint32_t i = 0; i < queue_family_properties.size(); ++i) {
        if ((queue_family_properties[i].queueFlags & vk::QueueFlagBits::eGraphics) && (physical_device.getSurfaceSupportKHR(i, *surface))) {
            queue_idx = i;
            break;
        }
    }
    if (queue_idx == ~0) {
        throw std::runtime_error("no suitable queue family found");
    }

    vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> device_features{
        {},
        {.dynamicRendering = true},
        {.extendedDynamicState = true}
    };

    float queue_priority = 0.5f;
    vk::DeviceQueueCreateInfo queue_create_info{.queueFamilyIndex = queue_idx, .queueCount = 1, .pQueuePriorities = &queue_priority};
    vk::DeviceCreateInfo device_create_info{
        .pNext = &device_features.get<vk::PhysicalDeviceFeatures2>(),
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_create_info,
        .enabledExtensionCount = static_cast<uint32_t>(required_extensions.size()),
        .ppEnabledExtensionNames = required_extensions.data()
    };
    device = vk::raii::Device(physical_device, device_create_info);
    queue = vk::raii::Queue(device, queue_idx, 0);

    // create the swapchain
    vk::SurfaceCapabilitiesKHR surface_capabilities = physical_device.getSurfaceCapabilitiesKHR(*surface);
    swapchain_extent = choose_swap_extent(surface_capabilities);
    uint32_t min_image_count = choose_min_image_count(surface_capabilities);

    std::vector<vk::SurfaceFormatKHR> surface_formats = physical_device.getSurfaceFormatsKHR(*surface);
    swapchain_surface_format = choose_swap_surface_format(surface_formats);

    std::vector<vk::PresentModeKHR> present_modes = physical_device.getSurfacePresentModesKHR(*surface);
    vk::PresentModeKHR present_mode = choose_present_mode(present_modes);

    vk::SwapchainCreateInfoKHR swapchain_create_info{
        .surface = *surface,
        .minImageCount = min_image_count,
        .imageFormat = swapchain_surface_format.format,
        .imageColorSpace = swapchain_surface_format.colorSpace,
        .imageExtent = swapchain_extent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .preTransform = surface_capabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = present_mode,
        .clipped = true
    };
    swapchain = vk::raii::SwapchainKHR(device, swapchain_create_info);
    swapchain_images = swapchain.getImages();

    // create the image views
    assert(swapchain_image_views.empty());
    vk::ImageViewCreateInfo image_view_create_info{
        .viewType = vk::ImageViewType::e2D,
        .format = swapchain_surface_format.format,
        .subresourceRange = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .levelCount = 1,
            .layerCount = 1
        }
    };
    for (auto& image : swapchain_images) {
        image_view_create_info.image = image;
        swapchain_image_views.emplace_back(device, image_view_create_info);
    }
}

}