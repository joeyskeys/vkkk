#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

//#define STB_IMAGE_IMPLEMENTATION
//#include <stb_image.h>

#include "vkabstraction.h"

namespace fs = std::filesystem;

namespace vkkk
{

inline constexpr int MAX_FRAMES_IN_FLIGHT = 2;

VkWrappedInstance::VkWrappedInstance()
    : window(nullptr)
{}

VkWrappedInstance::VkWrappedInstance(uint32_t w, uint32_t h, const std::string& appname, const std::string& enginename)
    : width(w)
    , height(h)
    , app_name(appname)
    , engine_name(enginename)
{}

VkWrappedInstance::~VkWrappedInstance() {
    cleanup_swapchain();

    if (syncobj_created) {
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            vkDestroySemaphore(device, render_finished_semaphores[i], nullptr);
            vkDestroySemaphore(device, image_available_semaphores[i], nullptr);
            vkDestroyFence(device, in_flight_fences[i], nullptr);
        }
    }

    vkDestroyDevice(device, nullptr);

    if (enable_validation_layers) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr)
            func(instance, debug_messenger, nullptr);
    }

    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);

    glfwDestroyWindow(window);
    glfwTerminate();
}

void VkWrappedInstance::list_physical_devices() const {
    VkPhysicalDeviceProperties props;
    if (physical_devices.size() > 0) {
        std::cout << "Devices available:" << std::endl;
        for (int i = 0; i < physical_devices.size(); ++i) {
            vkGetPhysicalDeviceProperties(physical_devices[i], &props);
            std::cout << i + 1 << ". " << props.deviceName << std::endl;
        }
    }
    else
        std::cout << "No physical devices stored in the list.." << std::endl;
}

void VkWrappedInstance::choose_device(uint32_t i) {
    --i;
    if (i < 0 || i >= physical_devices.size()) {
        std::cout << "Physical device index out of range, list the devices to checkout" << std::endl;
        return;
    }

    if (physical_devices.size() == 1) {
        std::cout << "Only one device available, default to it.." << std::endl;
        return;
    }

    physical_device = physical_devices[i];
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);
    std::cout << "Using device " << i + 1 << std::endl;
}

void VkWrappedInstance::init_glfw() {
    // Gui bounded to Vk for now
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(width, height, app_name.c_str(), nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, default_resize_callback);
}

void VkWrappedInstance::init() {
    // Init vulkan
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = app_name.c_str();
    app_info.applicationVersion = app_version;
    app_info.pEngineName = engine_name.c_str();
    app_info.apiVersion = api_version;

    VkInstanceCreateInfo instance_info{};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;

    auto extensions = get_default_instance_extensions();
    instance_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    instance_info.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
    if (enable_validation_layers) {
        instance_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        instance_info.ppEnabledExtensionNames = extensions.data();

        debug_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug_create_info.pfnUserCallback = default_debug_callback;
        instance_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debug_create_info;

    }
    else {
        instance_info.enabledExtensionCount = 0;
        instance_info.pNext = nullptr;
    }

    if (vkCreateInstance(&instance_info, nullptr, &instance) != VK_SUCCESS)
        throw std::runtime_error("failed to create instance..");

    if (enable_validation_layers) {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            instance, "vkCreateDebugUtilsMessengerEXT");
        VkResult ret = VK_ERROR_EXTENSION_NOT_PRESENT;
        if (func != nullptr) {
            auto ret = func(instance, &debug_create_info, nullptr, &debug_messenger);
            if (ret != VK_SUCCESS)
                throw std::runtime_error("failed to set up debug messenger..");
        }
    }

    uint32_t device_cnt = 0;
    vkEnumeratePhysicalDevices(instance, &device_cnt, nullptr);

    if (device_cnt > 0) {
        physical_devices.resize(device_cnt);
        vkEnumeratePhysicalDevices(instance, &device_cnt, physical_devices.data());
        physical_device = physical_devices[0];
        vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);
    }
    else {
        throw std::runtime_error("No physical device available for vulkan");
    }

    vkGetPhysicalDeviceProperties(physical_device, &physical_device_props);
}

VkCommandBuffer VkWrappedInstance::begin_single_time_commands() {
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = command_pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buf;
    vkAllocateCommandBuffers(device, &alloc_info, &command_buf);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(command_buf, &begin_info);
    return command_buf;
}

void VkWrappedInstance::end_single_time_commands(VkCommandBuffer cmd_buf) {
    vkEndCommandBuffer(cmd_buf);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buf;

    vkQueueSubmit(graphic_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphic_queue);
    vkFreeCommandBuffers(device, command_pool, 1, &cmd_buf);
}

void VkWrappedInstance::create_vk_image(const uint32_t w, const uint32_t h,
    const uint32_t layers, const VkSampleCountFlagBits n, const VkFormat format,
    VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags,
    VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& image_memo)
{
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = w;
    image_info.extent.height = h;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = layers;
    image_info.format = format;
    image_info.tiling = tiling;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage;
    image_info.samples = n;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.flags = flags;

    if (vkCreateImage(device, &image_info, nullptr, &image) != VK_SUCCESS)
        throw std::runtime_error("failed to create image");

    VkMemoryRequirements mem_reqs{};
    vkGetImageMemoryRequirements(device, image, &mem_reqs);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &alloc_info, nullptr, &image_memo) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate image memory");

    vkBindImageMemory(device, image, image_memo, 0);
}

void VkWrappedInstance::transition_image_layout(VkImage image, VkFormat format,
    VkImageLayout old_layout, VkImageLayout new_layout, VkImageSubresourceRange sub_range)
{
    VkCommandBuffer cmd_buf = begin_single_time_commands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    /*
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    */
    barrier.subresourceRange = sub_range;

    VkPipelineStageFlags src_stage;
    VkPipelineStageFlags dst_stage;

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else {
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

void VkWrappedInstance::copy_buffer_to_image(VkBuffer buf, VkImage image, const std::vector<VkBufferImageCopy>& regions)
{
    VkCommandBuffer cmd_buf = begin_single_time_commands();

    vkCmdCopyBufferToImage(cmd_buf, buf, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        regions.size(), regions.data());

    end_single_time_commands(cmd_buf);
}

void VkWrappedInstance::create_surface() {
    assert(window != nullptr);
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("failed to create window surface!");
}

VkSampleCountFlagBits VkWrappedInstance::get_max_usable_sample_cnt() const {
    VkSampleCountFlags cnts = physical_device_props.limits.framebufferColorSampleCounts
        & physical_device_props.limits.framebufferDepthSampleCounts;

    if (cnts & VK_SAMPLE_COUNT_64_BIT)
        return VK_SAMPLE_COUNT_64_BIT;
    if (cnts & VK_SAMPLE_COUNT_32_BIT)
        return VK_SAMPLE_COUNT_32_BIT;
    if (cnts & VK_SAMPLE_COUNT_16_BIT)
        return VK_SAMPLE_COUNT_16_BIT;
    if (cnts & VK_SAMPLE_COUNT_8_BIT)
        return VK_SAMPLE_COUNT_8_BIT;
    if (cnts & VK_SAMPLE_COUNT_4_BIT)
        return VK_SAMPLE_COUNT_4_BIT;
    if (cnts & VK_SAMPLE_COUNT_2_BIT)
        return VK_SAMPLE_COUNT_2_BIT;
    return VK_SAMPLE_COUNT_1_BIT;
}

bool VkWrappedInstance::validate_current_device(QueueFamilyIndex* idx) {
    if (idx == nullptr)
        return false;

    auto default_device_extensions = get_default_device_extensions();

    bool extensions_supported = check_device_extension_support(default_device_extensions);
    if (!extensions_supported)
        return false;

    bool swapchain_adequate = false;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &swapchain_details.capabilities);

    uint32_t format_cnt;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_cnt, nullptr);
    if (format_cnt != 0) {
        swapchain_details.formats.resize(format_cnt);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_cnt, swapchain_details.formats.data());
    }

    uint32_t present_mode_cnt;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_cnt, nullptr);
    if (present_mode_cnt != 0) {
        swapchain_details.present_modes.resize(present_mode_cnt);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_cnt, swapchain_details.present_modes.data());
    }

    if (swapchain_details.formats.empty() || swapchain_details.present_modes.empty())
        return false;

    uint32_t queue_family_cnt = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_cnt, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_cnt);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_cnt, queue_families.data());

    VkPhysicalDeviceFeatures supported_features;
    vkGetPhysicalDeviceFeatures(physical_device, &supported_features);
    if (!supported_features.samplerAnisotropy)
        return false;

    int i = 0;
    VkBool32 present_support = false;
    for (const auto& queue_family : queue_families) {
        if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            idx->graphic_family = i;

        vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &present_support);
        if (present_support)
            idx->present_family = i;

        if (idx->is_valid())
            return true;

        ++i;
    }

    return false;
}

void VkWrappedInstance::create_logical_device() {
    if (!validate_current_device(&queue_family_idx))
        throw std::runtime_error("Queue specified not available");

    // Queue create info
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::set<uint32_t> queue_families = {
        queue_family_idx.graphic_family.value(),
        queue_family_idx.present_family.value()
    };
    
    float queue_priority = 1.f;
    for (auto& queue_family : queue_families) {
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = queue_family;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(queue_create_info);
        queue_idx_vec.push_back(queue_family);
    }

    // Device feature
    VkPhysicalDeviceFeatures device_features{};
    device_features.samplerAnisotropy = VK_TRUE;

    // Device create info
    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    device_create_info.pQueueCreateInfos = queue_create_infos.data();
    device_create_info.pEnabledFeatures = &device_features;

    auto default_device_extensions = get_default_device_extensions();
    device_create_info.enabledExtensionCount = static_cast<uint32_t>(default_device_extensions.size());
    device_create_info.ppEnabledExtensionNames = default_device_extensions.data();

    // Modern impls will have this info from instance, ignore for now..
    // device_create_info.enabledExtensionCount = 0;
    // if (enable_validation_layers) {
    //  device_create_info.enabledLayerCount = validationLayers.size();
    //  device_create_info.ppEnabledLayerNames = validationLayers.data();
    // }
    // else
    //  device_create_info.enabledLayerCount = 0;

    // Create logical device
    if (vkCreateDevice(physical_device, &device_create_info, nullptr, &device) != VK_SUCCESS)
        throw std::runtime_error("failed to create logical device!");

    // Retrieve queue
    vkGetDeviceQueue(device, queue_family_idx.graphic_family.value(), 0, &graphic_queue);
    vkGetDeviceQueue(device, queue_family_idx.present_family.value(), 0, &present_queue);

    queue_created = true;
}

uint32_t VkWrappedInstance::create_swapchain() {
    if (!queue_created)
        throw std::runtime_error("Queue not created yet, cannot create swapchain");

    swapchain_surface_format = choose_swap_surface_format(swapchain_details.formats);
    VkPresentModeKHR present_mode = choose_swap_present_mode(swapchain_details.present_modes);
    swapchain_extent = choose_swap_extent(swapchain_details.capabilities);

    uint32_t image_cnt = swapchain_details.capabilities.minImageCount + 1;
    if (swapchain_details.capabilities.maxImageCount > 0 && image_cnt > swapchain_details.capabilities.maxImageCount)
        image_cnt = swapchain_details.capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface;
    
    create_info.minImageCount = image_cnt;
    create_info.imageFormat = swapchain_surface_format.format;
    create_info.imageColorSpace = swapchain_surface_format.colorSpace;
    create_info.imageExtent = swapchain_extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (queue_idx_vec.size() == 1)
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    else {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_idx_vec.data();
    }

    create_info.preTransform = swapchain_details.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;

    create_info.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &create_info, nullptr, &swapchain) != VK_SUCCESS)
        throw std::runtime_error("failed to create swap chain");

    vkGetSwapchainImagesKHR(device, swapchain, &image_cnt, nullptr);
    swapchain_images.resize(image_cnt);
    vkGetSwapchainImagesKHR(device, swapchain, &image_cnt, swapchain_images.data());

    swapchain_created = true;
    swapchain_cnt = image_cnt;
    return image_cnt;
}

void VkWrappedInstance::cleanup_swapchain() {
    if (color_created) {
        vkDestroyImageView(device, color_img_view, nullptr);
        vkDestroyImage(device, color_img, nullptr);
        vkFreeMemory(device, color_img_memo, nullptr);
    }
    if (depth_created) {
        vkDestroyImageView(device, depth_img_view, nullptr);
        vkDestroyImage(device, depth_img, nullptr);
        vkFreeMemory(device, depth_img_memo, nullptr);
    }

    if (framebuffer_created)
        for (auto framebuffer : swapchain_framebuffers)
            vkDestroyFramebuffer(device, framebuffer, nullptr);

    if (commandpool_created)
        vkFreeCommandBuffers(device, command_pool, static_cast<uint32_t>(commandbuffers.size()),
            commandbuffers.data());

    if (render_pass_created)
        vkDestroyRenderPass(device, render_pass, nullptr);

    if (imageviews_created)
        for (auto imageview : swapchain_imageviews)
            vkDestroyImageView(device, imageview, nullptr);

    if (swapchain_created)
        vkDestroySwapchainKHR(device, swapchain, nullptr);
}

void VkWrappedInstance::recreate_swapchain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device);

    cleanup_swapchain();

    create_swapchain();
    create_imageviews();
    create_renderpass();
    create_color_resource();
    create_depth_resource();
    create_framebuffers();

    images_in_flight.resize(swapchain_images.size(), VK_NULL_HANDLE);
}

VkImageView VkWrappedInstance::create_imageview(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags) {
    VkImageViewCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create_info.image = image;
    create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    create_info.format = format;

    //create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    //create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    //create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    //create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    
    create_info.subresourceRange.aspectMask = aspect_flags;
    create_info.subresourceRange.baseMipLevel = 0;
    create_info.subresourceRange.levelCount = 1;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount = 1;

    VkImageView image_view;
    if (vkCreateImageView(device, &create_info, nullptr, &image_view) != VK_SUCCESS)
        throw std::runtime_error("failed to create texture image view");

    return image_view;
}

void VkWrappedInstance::create_imageviews() {
    swapchain_imageviews.resize(swapchain_images.size());

    for (int i = 0; i < swapchain_images.size(); ++i)
        swapchain_imageviews[i] = create_imageview(swapchain_images[i], swapchain_surface_format.format, VK_IMAGE_ASPECT_COLOR_BIT);

    imageviews_created = true;
}

static std::vector<char> load_file(const fs::path& filepath) {
    if (!fs::exists(filepath))
        throw std::runtime_error(fmt::format("file {} not exists..", filepath.string()));

    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.good()) {
        //throw std::runtime_error(std::format("failed to open file : {}..", filepath));
        throw std::runtime_error("failed to open file..");
    }

    size_t file_size = fs::file_size(filepath);
    std::vector<char> buffer(file_size);
    file.seekg(0);
    file.read(buffer.data(), file_size);
    file.close();

    return buffer;
}

void VkWrappedInstance::create_renderpass() {
    VkAttachmentDescription attachment{};
    attachment.format = swapchain_surface_format.format;
    attachment.samples = nsample;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Final representation attachment remains 1 sample
    VkAttachmentDescription resolve_attachment{};
    resolve_attachment.format = swapchain_surface_format.format;
    resolve_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    resolve_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    resolve_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    resolve_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    resolve_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    resolve_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    resolve_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    VkAttachmentDescription depth_attach{};
    depth_attach.format = find_depth_format();
    depth_attach.samples = nsample;
    depth_attach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attach.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attach.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attach.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attach.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attach.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference attachment_ref{};
    attachment_ref.attachment = 0;
    attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference resolve_ref{};
    resolve_ref.attachment = 2;
    resolve_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attach_ref{};
    depth_attach_ref.attachment = 1;
    depth_attach_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &attachment_ref;
    subpass.pResolveAttachments = &resolve_ref;
    subpass.pDepthStencilAttachment = &depth_attach_ref;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 3> attachments = {attachment, depth_attach, resolve_attachment};
    VkRenderPassCreateInfo pass_info{};
    pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    pass_info.attachmentCount = attachments.size();
    pass_info.pAttachments = attachments.data();
    pass_info.subpassCount = 1;
    pass_info.pSubpasses = &subpass;
    pass_info.dependencyCount = 1;
    pass_info.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &pass_info, nullptr, &render_pass) != VK_SUCCESS)
        throw std::runtime_error("failed to create render pass!");

    render_pass_created = true;
}

void VkWrappedInstance::create_framebuffers() {
    swapchain_framebuffers.resize(swapchain_imageviews.size());

    for (int i = 0; i < swapchain_imageviews.size(); ++i) {
        std::array<VkImageView, 3> attachments = {
            color_img_view,
            depth_img_view,
            swapchain_imageviews[i],
        };

        VkFramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = render_pass;
        framebuffer_info.attachmentCount = attachments.size();
        framebuffer_info.pAttachments = attachments.data();
        framebuffer_info.width = swapchain_extent.width;
        framebuffer_info.height = swapchain_extent.height;
        framebuffer_info.layers = 1;

        if (vkCreateFramebuffer(device, &framebuffer_info, nullptr, &swapchain_framebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create framebuffer!");
    }

    framebuffer_created = true;
}

void VkWrappedInstance::create_command_pool() {
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = queue_family_idx.graphic_family.value();

    if (vkCreateCommandPool(device, &pool_info, nullptr, &command_pool) != VK_SUCCESS)
        throw std::runtime_error("failed to create command pool!");

    commandpool_created = true;
}

uint32_t VkWrappedInstance::find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);

    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_filter & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

void VkWrappedInstance::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags props, VkBuffer& buf, VkDeviceMemory& buf_memo) {
    VkBufferCreateInfo buf_info{};
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

void VkWrappedInstance::copy_buffer(VkBuffer src_buf, VkBuffer dst_buf, VkDeviceSize size) {
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = command_pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer cmd_buf;
    vkAllocateCommandBuffers(device, &alloc_info, &cmd_buf);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmd_buf, &begin_info);
        VkBufferCopy copy_region{};
        copy_region.size = size;
        vkCmdCopyBuffer(cmd_buf, src_buf, dst_buf, 1, &copy_region);
    vkEndCommandBuffer(cmd_buf);

    VkSubmitInfo sub_info{};
    sub_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    sub_info.commandBufferCount = 1;
    sub_info.pCommandBuffers = &cmd_buf;

    vkQueueSubmit(graphic_queue, 1, &sub_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphic_queue);

    vkFreeCommandBuffers(device, command_pool, 1, &cmd_buf);
}

void VkWrappedInstance::create_vertex_buffer(const float *source_data, VkBuffer& buf, VkDeviceMemory& memo, size_t comp_size, size_t vcnt) {
    VkDeviceSize buf_size = comp_size * vcnt * sizeof(float);
    VkBuffer staging_buf;
    VkDeviceMemory staging_buf_memo;
    create_buffer(buf_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buf, staging_buf_memo);

    void* data;
    vkMapMemory(device, staging_buf_memo, 0, buf_size, 0, &data);
        memcpy(data, source_data, buf_size);
    vkUnmapMemory(device, staging_buf_memo);

    create_buffer(buf_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buf, memo);

    copy_buffer(staging_buf, buf, buf_size);
    vkDestroyBuffer(device, staging_buf, nullptr);
    vkFreeMemory(device, staging_buf_memo, nullptr);
}

void VkWrappedInstance::create_index_buffer(const uint32_t* index_data, VkBuffer& buf, VkDeviceMemory& memo, size_t idx_cnt) {
    VkDeviceSize buf_size = sizeof(uint32_t) * idx_cnt;

    VkBuffer staging_buf;
    VkDeviceMemory staging_buf_memo;
    create_buffer(buf_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buf, staging_buf_memo);
    
    void* data;
    vkMapMemory(device, staging_buf_memo, 0, buf_size, 0, &data);
    memcpy(data, index_data, static_cast<size_t>(buf_size));
    vkUnmapMemory(device, staging_buf_memo);

    create_buffer(buf_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buf, memo);
    copy_buffer(staging_buf, buf, buf_size);

    vkDestroyBuffer(device, staging_buf, nullptr);
    vkFreeMemory(device, staging_buf_memo, nullptr);
}

VkFormat VkWrappedInstance::find_supported_format(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physical_device, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
            return format;
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
            return format;
    }

    throw std::runtime_error("failed to find supported format");
}

void VkWrappedInstance::create_color_resource() {
    VkFormat format = swapchain_surface_format.format;
    create_vk_image(swapchain_extent.width, swapchain_extent.height, 1,
        nsample, format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, color_img, color_img_memo);
    color_img_view = create_imageview(color_img, format, VK_IMAGE_ASPECT_COLOR_BIT);

    color_created = true;
}

void VkWrappedInstance::create_depth_resource() {
    VkFormat depth_format = find_depth_format();

    create_vk_image(swapchain_extent.width, swapchain_extent.height, 1, nsample,
        depth_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depth_img, depth_img_memo);
    depth_img_view = create_imageview(depth_img, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);

    depth_created = true;
}

void VkWrappedInstance::alloc_commandbuffers(std::vector<VkCommandBuffer>& bufs) {
    if (!commandpool_created)
        throw std::runtime_error("command pool not created!");

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = command_pool;
    // Make it a parameter?
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = bufs.size();

    if (vkAllocateCommandBuffers(device, &alloc_info, bufs.data()) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate command buffers");
}

void VkWrappedInstance::record_cmds(std::vector<VkCommandBuffer>& cmd_bufs,
    std::vector<VkFramebuffer>& fbs, const std::function<void(uint32_t)>& emit_func)
{
    auto swapchain_cnt = get_swapchain_cnt();
    assert(cmd_bufs.size() == swapchain_cnt);
    assert(fbs.size() == swapchain_cnt);

    for (int i = 0; i < swapchain_cnt; ++i) {
        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(cmd_bufs[i], &begin_info) != VK_SUCCESS)
            throw std::runtime_error("failed to begin recording command buffer");

        VkRenderPassBeginInfo renderpass_info{};
        renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderpass_info.renderPass = render_pass;
        renderpass_info.framebuffer = fbs[i];
        renderpass_info.renderArea.offset = { 0, 0 };
        renderpass_info.renderArea.extent = swapchain_extent;

        std::array<VkClearValue, 2> clear_values{};
        clear_values[0].color = {{ 0.f, 0.f, 0.f, 1.f }};
        clear_values[1].depthStencil = { 1.f, 0 };
        renderpass_info.clearValueCount = clear_values.size();
        renderpass_info.pClearValues = clear_values.data();

        vkCmdBeginRenderPass(cmd_bufs[i], &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);

            emit_func(i);

        vkCmdEndRenderPass(cmd_bufs[i]);        

        if (vkEndCommandBuffer(cmd_bufs[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to record command buffer!");
    }
}

void VkWrappedInstance::create_sync_objects() {
    image_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
    render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
    in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);
    images_in_flight.resize(swapchain_images.size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphor_info{};
    semaphor_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateSemaphore(device, &semaphor_info, nullptr, &image_available_semaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphor_info, nullptr, &render_finished_semaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fence_info, nullptr, &in_flight_fences[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create synchronization objects for a frame");
    }
}

void VkWrappedInstance::draw_frame(const CommandBuffers& cmd_bufs) {
    vkWaitForFences(device, 1, &in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);

    uint32_t image_idx;
    auto result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
        image_available_semaphores[current_frame], VK_NULL_HANDLE, &image_idx);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("failed to acquire swap chain image!");

    if (images_in_flight[image_idx] != VK_NULL_HANDLE)
        vkWaitForFences(device, 1, &images_in_flight[image_idx], VK_TRUE, UINT64_MAX);
    images_in_flight[image_idx] = in_flight_fences[current_frame];

    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - time);
    time = now;

    //update_uniform_buffer(image_idx);
    if (update_cbk)
        update_cbk(image_idx, duration.count());

    vkResetFences(device, 1, &in_flight_fences[current_frame]);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore wait_semaphores[] = { image_available_semaphores[current_frame] };
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;

    // Multiple cmds, possible usage?
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_bufs.bufs[image_idx];

    VkSemaphore signal_semaphores[] = { render_finished_semaphores[current_frame] };
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    if (vkQueueSubmit(graphic_queue, 1, &submit_info, in_flight_fences[current_frame]) != VK_SUCCESS)
        throw std::runtime_error("failed to submit draw command buffer!");

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;

    VkSwapchainKHR swapchains_for_present[] = { swapchain };
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchains_for_present;

    present_info.pImageIndices = &image_idx;

    result = vkQueuePresentKHR(present_queue, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebuffer_resized) {
        framebuffer_resized = false;
        recreate_swapchain();
    }
    else if (result != VK_SUCCESS)
        throw std::runtime_error("failed to present swap chain image!");

    current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VkWrappedInstance::mainloop(const CommandBuffers& cmd_bufs) {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        draw_frame(cmd_bufs);
    }

    vkDeviceWaitIdle(device);
}

std::vector<const char*> VkWrappedInstance::get_default_instance_extensions() {
    uint32_t glfw_extension_cnt = 0;
    const char** glfw_extensions;
    glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_cnt);
    std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_cnt);
    if (enable_validation_layers)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    //extensions.push_back("VK_LAYER_KHRONOS_validation");
    
    return extensions;
}

bool VkWrappedInstance::check_device_extension_support(const std::span<const char*> extensions) const {
    uint32_t extension_cnt;
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_cnt, nullptr);
    std::vector<VkExtensionProperties> available_extensions(extension_cnt);
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_cnt, available_extensions.data());

    std::set<std::string> required_extensions(extensions.begin(), extensions.end());
    for (const auto& extension : available_extensions) {
        required_extensions.erase(extension.extensionName);
    }
    return required_extensions.empty();
}

VkSurfaceFormatKHR VkWrappedInstance::choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats) const {
    for (const auto& available_format : available_formats) {
        if (available_format.format == VK_FORMAT_R8G8B8A8_SRGB && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return available_format;
    }

    return available_formats[0];
}

VkPresentModeKHR VkWrappedInstance::choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available_present_modes) const {
    for (const auto& available_present_mode : available_present_modes) {
        if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
            return available_present_mode;
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VkWrappedInstance::choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities) const {
    if (capabilities.currentExtent.width != UINT32_MAX)
        return capabilities.currentExtent;
    else {
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        VkExtent2D actual_extent = {
            static_cast<uint32_t>(w),
            static_cast<uint32_t>(h)
        };

        actual_extent.width = std::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actual_extent.height = std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return actual_extent;
    }
}

void VkWrappedInstance::setup_key_cbk(KeyCBK cbk) {
    glfwSetKeyCallback(window, cbk);
}

void VkWrappedInstance::setup_mouse_btn_cbk(MouseBtnCBK cbk) {
    glfwSetMouseButtonCallback(window, cbk);
}

void VkWrappedInstance::setup_mouse_pos_cbk(MousePosCBK cbk) {
    glfwSetCursorPosCallback(window, cbk);
}

std::unique_ptr<char[]> VkWrappedInstance::get_image_buffer(const RenderTarget& rt) const {
    VkImageSubresource subresource { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
    VkSubresourceLayout subresource_layout;
    vkGetImageSubresourceLayout(device, rt.image, &subresource, &subresource_layout);

    char* data;
    vkMapMemory(device, rt.memo, 0, VK_WHOLE_SIZE, 0, reinterpret_cast<void**>(&data));
    data += subresource_layout.offset;

    vkUnmapMemory(device, rt.memo, nullptr);
}

}