#include "vk_ins/vk_abc.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>

namespace vkkk
{

namespace
{

#ifdef NDEBUG
constexpr bool kEnableValidation = false;
#else
constexpr bool kEnableValidation = true;
#endif

#define VK_CHECK(expr, msg)                                                                 \
    do {                                                                                    \
        const VkResult _result = (expr);                                                    \
        if (_result != VK_SUCCESS) {                                                        \
            throw std::runtime_error(std::string(msg) + " (VkResult=" + std::to_string(_result) + ")"); \
        }                                                                                   \
    } while (false)

} // namespace

void ModernMeshGPU::upload(const Mesh& mesh, VkDevice device, VkPhysicalDevice physical_device,
    VkCommandPool command_pool, VkQueue queue)
{
    if (!mesh.loaded) {
        throw std::runtime_error("cannot upload unloaded mesh");
    }

    auto find_memory_type = [&](uint32_t filter, VkMemoryPropertyFlags properties) -> uint32_t {
        VkPhysicalDeviceMemoryProperties mem_props{};
        vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);
        for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
            if ((filter & (1u << i)) && (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        throw std::runtime_error("failed to find suitable memory type");
    };

    auto create_device_local_buffer = [&](VkDeviceSize size, VkBufferUsageFlags usage,
                                          const void* src_data, VkBuffer& out_buffer,
                                          VkDeviceMemory& out_memory) {
        VkBuffer staging_buffer = VK_NULL_HANDLE;
        VkDeviceMemory staging_memory = VK_NULL_HANDLE;

        VkBufferCreateInfo staging_info{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        VK_CHECK(vkCreateBuffer(device, &staging_info, nullptr, &staging_buffer), "create staging buffer");

        VkMemoryRequirements staging_req{};
        vkGetBufferMemoryRequirements(device, staging_buffer, &staging_req);
        VkMemoryAllocateInfo staging_alloc{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = staging_req.size,
            .memoryTypeIndex = find_memory_type(staging_req.memoryTypeBits,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
        };
        VK_CHECK(vkAllocateMemory(device, &staging_alloc, nullptr, &staging_memory), "allocate staging memory");
        VK_CHECK(vkBindBufferMemory(device, staging_buffer, staging_memory, 0), "bind staging memory");

        void* mapped = nullptr;
        VK_CHECK(vkMapMemory(device, staging_memory, 0, size, 0, &mapped), "map staging memory");
        std::memcpy(mapped, src_data, static_cast<size_t>(size));
        vkUnmapMemory(device, staging_memory);

        VkBufferCreateInfo buffer_info{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        VK_CHECK(vkCreateBuffer(device, &buffer_info, nullptr, &out_buffer), "create device buffer");

        VkMemoryRequirements buffer_req{};
        vkGetBufferMemoryRequirements(device, out_buffer, &buffer_req);
        VkMemoryAllocateInfo buffer_alloc{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = buffer_req.size,
            .memoryTypeIndex = find_memory_type(buffer_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        };
        VK_CHECK(vkAllocateMemory(device, &buffer_alloc, nullptr, &out_memory), "allocate device memory");
        VK_CHECK(vkBindBufferMemory(device, out_buffer, out_memory, 0), "bind device memory");

        VkCommandBufferAllocateInfo cmd_alloc{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VK_CHECK(vkAllocateCommandBuffers(device, &cmd_alloc, &cmd), "allocate upload command buffer");

        VkCommandBufferBeginInfo begin_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info), "begin upload command buffer");

        VkBufferCopy copy_region{.size = size};
        vkCmdCopyBuffer(cmd, staging_buffer, out_buffer, 1, &copy_region);
        VK_CHECK(vkEndCommandBuffer(cmd), "end upload command buffer");

        VkSubmitInfo submit_info{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd,
        };
        VK_CHECK(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE), "submit upload command buffer");
        vkQueueWaitIdle(queue);

        vkFreeCommandBuffers(device, command_pool, 1, &cmd);
        vkDestroyBuffer(device, staging_buffer, nullptr);
        vkFreeMemory(device, staging_memory, nullptr);
    };

    const VkDeviceSize vertex_bytes = mesh.comp_size * mesh.vcnt * sizeof(float);
    const VkDeviceSize index_bytes = mesh.icnt * 3 * sizeof(uint32_t);
    create_device_local_buffer(vertex_bytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, mesh.vbuf,
        vertex_buffer, vertex_memory);
    create_device_local_buffer(index_bytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, mesh.ibuf,
        index_buffer, index_memory);
    index_count = mesh.icnt;
}

void ModernMeshGPU::draw(VkCommandBuffer cmd, VkPipelineLayout layout,
    VkDescriptorSet descriptor_set) const
{
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_buffer, &offset);
    vkCmdBindIndexBuffer(cmd, index_buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descriptor_set, 0, nullptr);
    vkCmdDrawIndexed(cmd, index_count * 3, 1, 0, 0, 0);
}

void ModernMeshGPU::destroy(VkDevice device)
{
    if (index_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, index_buffer, nullptr);
        index_buffer = VK_NULL_HANDLE;
    }
    if (index_memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, index_memory, nullptr);
        index_memory = VK_NULL_HANDLE;
    }
    if (vertex_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, vertex_buffer, nullptr);
        vertex_buffer = VK_NULL_HANDLE;
    }
    if (vertex_memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, vertex_memory, nullptr);
        vertex_memory = VK_NULL_HANDLE;
    }
}

VkWrappedInstanceModern::VkWrappedInstanceModern(uint32_t width, uint32_t height, std::string app_name)
    : width_(width)
    , height_(height)
    , app_name_(std::move(app_name))
{}

VkWrappedInstanceModern::~VkWrappedInstanceModern()
{
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }

    for (auto& [name, mesh] : meshes_) {
        mesh.destroy(device_);
    }
    meshes_.clear();

    for (auto& [name, ubo] : uniform_buffers_) {
        for (uint32_t i = 0; i < ubo.buffers.size(); ++i) {
            vkDestroyBuffer(device_, ubo.buffers[i], nullptr);
            vkFreeMemory(device_, ubo.memories[i], nullptr);
        }
    }
    uniform_buffers_.clear();

    for (auto& [name, pipeline] : pipelines_) {
        if (pipeline.descriptor_pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device_, pipeline.descriptor_pool, nullptr);
        }
        if (pipeline.pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device_, pipeline.pipeline, nullptr);
        }
        if (pipeline.layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device_, pipeline.layout, nullptr);
        }
        if (pipeline.descriptor_layout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device_, pipeline.descriptor_layout, nullptr);
        }
    }
    pipelines_.clear();

    destroy_swapchain_targets();

    for (VkFence fence : in_flight_fences_) {
        vkDestroyFence(device_, fence, nullptr);
    }
    for (VkSemaphore semaphore : image_available_semaphores_) {
        vkDestroySemaphore(device_, semaphore, nullptr);
    }
    for (VkSemaphore semaphore : render_finished_semaphores_) {
        vkDestroySemaphore(device_, semaphore, nullptr);
    }

    if (command_pool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, command_pool_, nullptr);
    }

    if (device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(device_, nullptr);
    }

    if (kEnableValidation && debug_messenger_ != VK_NULL_HANDLE) {
        auto destroy_debug = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroy_debug != nullptr) {
            destroy_debug(instance_, debug_messenger_, nullptr);
        }
    }

    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
    }

    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
        glfwTerminate();
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL VkWrappedInstanceModern::debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* /*user_data*/)
{
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "[vk modern] " << callback_data->pMessage << std::endl;
    }
    return VK_FALSE;
}

void VkWrappedInstanceModern::framebuffer_resize_callback(GLFWwindow* window, int /*width*/, int /*height*/)
{
    auto* self = static_cast<VkWrappedInstanceModern*>(glfwGetWindowUserPointer(window));
    if (self != nullptr) {
        self->framebuffer_resized_ = true;
    }
}

std::vector<const char*> VkWrappedInstanceModern::required_instance_extensions() const
{
    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
    std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);
    if (kEnableValidation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return extensions;
}

std::vector<const char*> VkWrappedInstanceModern::required_device_extensions() const
{
    return {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
}

void VkWrappedInstanceModern::init_glfw()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    window_ = glfwCreateWindow(static_cast<int>(width_), static_cast<int>(height_),
        app_name_.c_str(), nullptr, nullptr);
    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebuffer_resize_callback);
}

void VkWrappedInstanceModern::init()
{
    create_instance();
    create_surface();
    if (!pick_physical_device()) {
        throw std::runtime_error("no suitable Vulkan 1.3 device with dynamic rendering support");
    }
    create_device();
}

void VkWrappedInstanceModern::create_instance()
{
    VkApplicationInfo app_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = app_name_.c_str(),
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "vkkk",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = kApiVersion,
    };

    const auto extensions = required_instance_extensions();

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
        .pfnUserCallback = debug_callback,
    };

    VkInstanceCreateInfo instance_info{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = kEnableValidation ? static_cast<void*>(&debug_create_info) : nullptr,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };

    VK_CHECK(vkCreateInstance(&instance_info, nullptr, &instance_), "create Vulkan instance");

    if (kEnableValidation) {
        auto create_debug = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
        if (create_debug != nullptr) {
            VK_CHECK(create_debug(instance_, &debug_create_info, nullptr, &debug_messenger_),
                "create debug messenger");
        }
    }
}

bool VkWrappedInstanceModern::pick_physical_device()
{
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);
    if (device_count == 0) {
        return false;
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance_, &device_count, devices.data());

    for (VkPhysicalDevice candidate : devices) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(candidate, &properties);
        if (properties.apiVersion < kApiVersion) {
            continue;
        }

        VkPhysicalDeviceVulkan13Features features_13{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        };
        VkPhysicalDeviceFeatures2 features_2{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &features_13,
        };
        vkGetPhysicalDeviceFeatures2(candidate, &features_2);
        if (!features_13.dynamicRendering) {
            continue;
        }

        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queue_family_count, nullptr);
        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queue_family_count, queue_families.data());

        std::optional<uint32_t> graphics_family;
        std::optional<uint32_t> present_family;
        for (uint32_t i = 0; i < queue_family_count; ++i) {
            if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphics_family = i;
            }
            VkBool32 present_support = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, surface_, &present_support);
            if (present_support == VK_TRUE) {
                present_family = i;
            }
        }
        if (!graphics_family.has_value() || !present_family.has_value()) {
            continue;
        }

        uint32_t extension_count = 0;
        vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extension_count, nullptr);
        std::vector<VkExtensionProperties> available_extensions(extension_count);
        vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extension_count, available_extensions.data());
        std::set<std::string> required(required_device_extensions().begin(), required_device_extensions().end());
        for (const auto& extension : available_extensions) {
            required.erase(extension.extensionName);
        }
        if (!required.empty()) {
            continue;
        }

        physical_device_ = candidate;
        graphics_queue_family_ = graphics_family.value();
        present_queue_family_ = present_family.value();
        return true;
    }

    return false;
}

void VkWrappedInstanceModern::create_device()
{
    const std::set<uint32_t> unique_queue_families{graphics_queue_family_, present_queue_family_};
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    float queue_priority = 1.0f;
    for (uint32_t queue_family : unique_queue_families) {
        queue_create_infos.push_back(VkDeviceQueueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = queue_family,
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
        });
    }

    VkPhysicalDeviceFeatures2 device_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .features = {.samplerAnisotropy = VK_TRUE},
    };

    VkPhysicalDeviceVulkan13Features features_13{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
    };
    device_features.pNext = &features_13;

    const auto device_extensions = required_device_extensions();
    VkDeviceCreateInfo device_info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &device_features,
        .queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size()),
        .pQueueCreateInfos = queue_create_infos.data(),
        .enabledExtensionCount = static_cast<uint32_t>(device_extensions.size()),
        .ppEnabledExtensionNames = device_extensions.data(),
    };

    VK_CHECK(vkCreateDevice(physical_device_, &device_info, nullptr, &device_), "create logical device");
    vkGetDeviceQueue(device_, graphics_queue_family_, 0, &graphics_queue_);
    vkGetDeviceQueue(device_, present_queue_family_, 0, &present_queue_);
}

void VkWrappedInstanceModern::create_surface()
{
    VK_CHECK(glfwCreateWindowSurface(instance_, window_, nullptr, &surface_), "create window surface");
}

VkSurfaceFormatKHR VkWrappedInstanceModern::choose_surface_format(
    const std::vector<VkSurfaceFormatKHR>& formats) const
{
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB
            && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return formats.front();
}

VkPresentModeKHR VkWrappedInstanceModern::choose_present_mode(
    const std::vector<VkPresentModeKHR>& modes) const
{
    for (VkPresentModeKHR mode : modes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VkWrappedInstanceModern::choose_extent(const VkSurfaceCapabilitiesKHR& capabilities) const
{
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }

    int framebuffer_width = 0;
    int framebuffer_height = 0;
    glfwGetFramebufferSize(window_, &framebuffer_width, &framebuffer_height);

    VkExtent2D extent{
        .width = static_cast<uint32_t>(framebuffer_width),
        .height = static_cast<uint32_t>(framebuffer_height),
    };
    extent.width = std::clamp(extent.width, capabilities.minImageExtent.width,
        capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height, capabilities.minImageExtent.height,
        capabilities.maxImageExtent.height);
    return extent;
}

void VkWrappedInstanceModern::create_swapchain_targets()
{
    VkSurfaceCapabilitiesKHR capabilities{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_, &capabilities);

    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &format_count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &format_count, formats.data());

    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &present_mode_count, nullptr);
    std::vector<VkPresentModeKHR> present_modes(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &present_mode_count,
        present_modes.data());

    const VkSurfaceFormatKHR surface_format = choose_surface_format(formats);
    const VkPresentModeKHR present_mode = choose_present_mode(present_modes);
    swapchain_extent_ = choose_extent(capabilities);
    width_ = swapchain_extent_.width;
    height_ = swapchain_extent_.height;
    swapchain_format_ = surface_format.format;

    uint32_t image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
        image_count = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchain_info{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface_,
        .minImageCount = image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = swapchain_extent_,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
    };

    const std::array<uint32_t, 2> queue_family_indices{graphics_queue_family_, present_queue_family_};
    if (graphics_queue_family_ != present_queue_family_) {
        swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_info.queueFamilyIndexCount = static_cast<uint32_t>(queue_family_indices.size());
        swapchain_info.pQueueFamilyIndices = queue_family_indices.data();
    }

    VK_CHECK(vkCreateSwapchainKHR(device_, &swapchain_info, nullptr, &swapchain_), "create swapchain");

    uint32_t swapchain_image_count = 0;
    vkGetSwapchainImagesKHR(device_, swapchain_, &swapchain_image_count, nullptr);
    swapchain_images_.resize(swapchain_image_count);
    vkGetSwapchainImagesKHR(device_, swapchain_, &swapchain_image_count, swapchain_images_.data());

    swapchain_image_views_.resize(swapchain_image_count);
    for (uint32_t i = 0; i < swapchain_image_count; ++i) {
        VkImageViewCreateInfo view_info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchain_images_[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapchain_format_,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        VK_CHECK(vkCreateImageView(device_, &view_info, nullptr, &swapchain_image_views_[i]),
            "create swapchain image view");
    }

    depth_format_ = VK_FORMAT_D32_SFLOAT;
    {
        const std::array candidates{
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT,
        };
        for (VkFormat candidate : candidates) {
            VkFormatProperties props{};
            vkGetPhysicalDeviceFormatProperties(physical_device_, candidate, &props);
            if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                depth_format_ = candidate;
                break;
            }
        }
    }

    create_depth_resources();

    VkCommandPoolCreateInfo pool_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphics_queue_family_,
    };
    VK_CHECK(vkCreateCommandPool(device_, &pool_info, nullptr, &command_pool_), "create command pool");

    allocate_command_buffers();
    images_in_flight_.assign(swapchain_image_count, VK_NULL_HANDLE);
}

void VkWrappedInstanceModern::create_depth_resources()
{
    depth_images_.resize(swapchain_images_.size());
    depth_memories_.resize(swapchain_images_.size());
    depth_image_views_.resize(swapchain_images_.size());

    for (uint32_t i = 0; i < swapchain_images_.size(); ++i) {
        VkImageCreateInfo image_info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = depth_format_,
            .extent = {swapchain_extent_.width, swapchain_extent_.height, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };
        VK_CHECK(vkCreateImage(device_, &image_info, nullptr, &depth_images_[i]), "create depth image");

        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device_, depth_images_[i], &requirements);
        VkMemoryAllocateInfo alloc_info{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = requirements.size,
            .memoryTypeIndex = find_memory_type(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        };
        VK_CHECK(vkAllocateMemory(device_, &alloc_info, nullptr, &depth_memories_[i]), "allocate depth memory");
        VK_CHECK(vkBindImageMemory(device_, depth_images_[i], depth_memories_[i], 0), "bind depth memory");

        VkImageViewCreateInfo view_info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = depth_images_[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = depth_format_,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        VK_CHECK(vkCreateImageView(device_, &view_info, nullptr, &depth_image_views_[i]),
            "create depth image view");
    }
}

void VkWrappedInstanceModern::allocate_command_buffers()
{
    command_buffers_.resize(swapchain_images_.size());
    VkCommandBufferAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool_,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(command_buffers_.size()),
    };
    VK_CHECK(vkAllocateCommandBuffers(device_, &alloc_info, command_buffers_.data()),
        "allocate command buffers");
}

void VkWrappedInstanceModern::create_sync_objects()
{
    image_available_semaphores_.resize(kMaxFramesInFlight);
    render_finished_semaphores_.resize(kMaxFramesInFlight);
    in_flight_fences_.resize(kMaxFramesInFlight);

    VkSemaphoreCreateInfo semaphore_info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fence_info{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        VK_CHECK(vkCreateSemaphore(device_, &semaphore_info, nullptr, &image_available_semaphores_[i]),
            "create image-available semaphore");
        VK_CHECK(vkCreateSemaphore(device_, &semaphore_info, nullptr, &render_finished_semaphores_[i]),
            "create render-finished semaphore");
        VK_CHECK(vkCreateFence(device_, &fence_info, nullptr, &in_flight_fences_[i]), "create fence");
    }

    last_frame_time_ = std::chrono::steady_clock::now();
}

uint32_t VkWrappedInstanceModern::find_memory_type(uint32_t filter, VkMemoryPropertyFlags properties) const
{
    VkPhysicalDeviceMemoryProperties mem_props{};
    vkGetPhysicalDeviceMemoryProperties(physical_device_, &mem_props);
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((filter & (1u << i)) && (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type");
}

void VkWrappedInstanceModern::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory) const
{
    VkBufferCreateInfo buffer_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VK_CHECK(vkCreateBuffer(device_, &buffer_info, nullptr, &buffer), "create buffer");

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device_, buffer, &requirements);
    VkMemoryAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = find_memory_type(requirements.memoryTypeBits, properties),
    };
    VK_CHECK(vkAllocateMemory(device_, &alloc_info, nullptr, &memory), "allocate buffer memory");
    VK_CHECK(vkBindBufferMemory(device_, buffer, memory, 0), "bind buffer memory");
}

void VkWrappedInstanceModern::copy_buffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) const
{
    VkCommandBuffer cmd = begin_one_shot_commands();
    VkBufferCopy region{.size = size};
    vkCmdCopyBuffer(cmd, src, dst, 1, &region);
    end_one_shot_commands(cmd);
}

VkCommandBuffer VkWrappedInstanceModern::begin_one_shot_commands() const
{
    VkCommandBufferAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool_,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(device_, &alloc_info, &cmd), "allocate one-shot command buffer");

    VkCommandBufferBeginInfo begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info), "begin one-shot command buffer");
    return cmd;
}

void VkWrappedInstanceModern::end_one_shot_commands(VkCommandBuffer cmd) const
{
    VK_CHECK(vkEndCommandBuffer(cmd), "end one-shot command buffer");
    VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
    };
    VK_CHECK(vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE), "submit one-shot command buffer");
    vkQueueWaitIdle(graphics_queue_);
    vkFreeCommandBuffers(device_, command_pool_, 1, &cmd);
}

void VkWrappedInstanceModern::transition_image_layout(VkCommandBuffer cmd, VkImage image,
    VkImageLayout old_layout, VkImageLayout new_layout, VkImageAspectFlags aspect_mask)
{
    VkImageMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = 0,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = aspect_mask,
            .levelCount = 1,
            .layerCount = 1,
        },
    };

    VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else if (old_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        && new_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = 0;
        src_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }

    vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

bool VkWrappedInstanceModern::create_graphics_pipeline(const std::string& name, ModernGraphicsPipelineDesc desc)
{
    if (pipelines_.find(name) != pipelines_.end()) {
        return true;
    }

    if (desc.shader_modules.size() < 2) {
        return false;
    }

    std::vector<VkShaderModule> shader_modules;
    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
    std::vector<VkDescriptorSetLayoutBinding> descriptor_bindings;
    std::map<uint32_t, std::string> ubo_binding_to_name;
    std::map<uint32_t, std::string> tex_binding_to_name;

    shader_modules.reserve(desc.shader_modules.size());
    shader_stages.reserve(desc.shader_modules.size());

    for (auto& module : desc.shader_modules) {
        VkShaderModuleCreateInfo module_info{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = module.spirv_code.size() * sizeof(uint32_t),
            .pCode = module.spirv_code.data(),
        };
        VkShaderModule vk_module = VK_NULL_HANDLE;
        VK_CHECK(vkCreateShaderModule(device_, &module_info, nullptr, &vk_module), "create shader module");
        shader_modules.push_back(vk_module);

        shader_stages.push_back(VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = module.type,
            .module = vk_module,
            .pName = "main",
        });

        for (const auto& [ubo_name, ubo_info] : module.buf_infos) {
            const auto& [struct_size, array_size, binding] = ubo_info;
            const std::string full_name = name + ":" + ubo_name;
            if (uniform_buffers_.find(full_name) == uniform_buffers_.end()) {
                ModernUBO ubo{};
                ubo.size = struct_size;
                ubo.vecsize = array_size;
                ubo.buffers.resize(swapchain_images_.size());
                ubo.memories.resize(swapchain_images_.size());
                for (uint32_t i = 0; i < swapchain_images_.size(); ++i) {
                    create_buffer(struct_size * array_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        ubo.buffers[i], ubo.memories[i]);
                }
                uniform_buffers_.emplace(full_name, std::move(ubo));
            }

            ubo_binding_to_name[binding] = full_name;
            descriptor_bindings.push_back(VkDescriptorSetLayoutBinding{
                .binding = binding,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = array_size,
                .stageFlags = static_cast<VkShaderStageFlags>(module.type),
            });
        }
    }

    VkDescriptorSetLayoutCreateInfo descriptor_layout_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(descriptor_bindings.size()),
        .pBindings = descriptor_bindings.data(),
    };
    VkDescriptorSetLayout descriptor_layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(device_, &descriptor_layout_info, nullptr, &descriptor_layout),
        "create descriptor set layout");

    VkPipelineLayoutCreateInfo layout_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptor_layout,
    };
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(device_, &layout_info, nullptr, &pipeline_layout), "create pipeline layout");

    std::vector<VkVertexInputBindingDescription> binding_descriptions;
    std::vector<VkVertexInputAttributeDescription> attribute_descriptions;
    for (auto& module : desc.shader_modules) {
        if (module.type != VK_SHADER_STAGE_VERTEX_BIT) {
            continue;
        }
        for (const auto& [binding, attrs] : module.input_brefs) {
            uint32_t offset = 0;
            for (uint32_t location : attrs) {
                const auto& attr = module.attr_infos.at(location);
                const auto glsl_type = std::get<1>(attr);
                attribute_descriptions.push_back(VkVertexInputAttributeDescription{
                    .location = location,
                    .binding = binding,
                    .format = glsl_type_macro[glsl_type],
                    .offset = offset,
                });
                offset += glsl_type_sizes[glsl_type];
            }
            uint32_t stride = 0;
            for (const auto component : desc.vertex_components) {
                stride += comp_sizes[component] * static_cast<uint32_t>(sizeof(float));
            }
            binding_descriptions.push_back(VkVertexInputBindingDescription{
                .binding = binding,
                .stride = stride,
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            });
        }
    }

    const VkFormat color_format = swapchain_format_;
    VkPipelineRenderingCreateInfo rendering_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &color_format,
        .depthAttachmentFormat = depth_format_,
    };

    const std::array dynamic_states{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamic_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
        .pDynamicStates = dynamic_states.data(),
    };

    VkPipelineVertexInputStateCreateInfo vertex_input{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = static_cast<uint32_t>(binding_descriptions.size()),
        .pVertexBindingDescriptions = binding_descriptions.data(),
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size()),
        .pVertexAttributeDescriptions = attribute_descriptions.data(),
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    VkPipelineViewportStateCreateInfo viewport_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo rasterization{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = desc.cull_mode,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisampling{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkPipelineDepthStencilStateCreateInfo depth_stencil{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = desc.depth_test ? VK_TRUE : VK_FALSE,
        .depthWriteEnable = desc.depth_write ? VK_TRUE : VK_FALSE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
    };

    VkPipelineColorBlendAttachmentState blend_attachment{
        .blendEnable = desc.blend_enable ? VK_TRUE : VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo color_blend{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blend_attachment,
    };

    VkGraphicsPipelineCreateInfo pipeline_info{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering_info,
        .stageCount = static_cast<uint32_t>(shader_stages.size()),
        .pStages = shader_stages.data(),
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterization,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depth_stencil,
        .pColorBlendState = &color_blend,
        .pDynamicState = &dynamic_state,
        .layout = pipeline_layout,
    };

    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline),
        "create graphics pipeline");

    for (VkShaderModule module : shader_modules) {
        vkDestroyShaderModule(device_, module, nullptr);
    }

    const uint32_t descriptor_set_count = static_cast<uint32_t>(swapchain_images_.size());
    std::vector<VkDescriptorPoolSize> pool_sizes;
    if (!ubo_binding_to_name.empty()) {
        pool_sizes.push_back(VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = static_cast<uint32_t>(ubo_binding_to_name.size()) * descriptor_set_count,
        });
    }

    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptor_sets(descriptor_set_count);
    if (!pool_sizes.empty()) {
        VkDescriptorPoolCreateInfo pool_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = descriptor_set_count,
            .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
            .pPoolSizes = pool_sizes.data(),
        };
        VK_CHECK(vkCreateDescriptorPool(device_, &pool_info, nullptr, &descriptor_pool), "create descriptor pool");

        std::vector<VkDescriptorSetLayout> layouts(descriptor_set_count, descriptor_layout);
        VkDescriptorSetAllocateInfo alloc_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = descriptor_pool,
            .descriptorSetCount = descriptor_set_count,
            .pSetLayouts = layouts.data(),
        };
        VK_CHECK(vkAllocateDescriptorSets(device_, &alloc_info, descriptor_sets.data()),
            "allocate descriptor sets");

        for (uint32_t i = 0; i < descriptor_set_count; ++i) {
            std::vector<VkWriteDescriptorSet> writes;
            std::vector<VkDescriptorBufferInfo> buffer_infos;
            buffer_infos.reserve(ubo_binding_to_name.size());
            writes.reserve(ubo_binding_to_name.size());

            for (const auto& [binding, ubo_name] : ubo_binding_to_name) {
                auto& ubo = uniform_buffers_.at(ubo_name);
                buffer_infos.push_back(VkDescriptorBufferInfo{
                    .buffer = ubo.buffers[i],
                    .offset = 0,
                    .range = ubo.size * ubo.vecsize,
                });
                writes.push_back(VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = descriptor_sets[i],
                    .dstBinding = binding,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .pBufferInfo = &buffer_infos.back(),
                });
            }
            vkUpdateDescriptorSets(device_, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }
    }

    pipelines_.emplace(name, ModernPipeline{
        .pipeline = pipeline,
        .layout = pipeline_layout,
        .descriptor_layout = descriptor_layout,
        .descriptor_pool = descriptor_pool,
        .descriptor_sets = std::move(descriptor_sets),
    });

    return true;
}

bool VkWrappedInstanceModern::load_mesh(const std::string& name, const Mesh& mesh)
{
    ModernMeshGPU gpu{};
    gpu.upload(mesh, device_, physical_device_, command_pool_, graphics_queue_);
    meshes_.insert_or_assign(name, std::move(gpu));
    return true;
}

ModernUBO& VkWrappedInstanceModern::uniform_buffer(const std::string& name)
{
    auto found = uniform_buffers_.find(name);
    if (found == uniform_buffers_.end()) {
        throw std::runtime_error("uniform buffer not found: " + name);
    }
    return found->second;
}

void VkWrappedInstanceModern::write_uniform(const std::string& name, uint32_t swapchain_index,
    const void* data, uint32_t size)
{
    auto& ubo = uniform_buffer(name);
    if (swapchain_index >= ubo.buffers.size()) {
        throw std::runtime_error("swapchain index out of range for uniform buffer");
    }
    void* mapped = nullptr;
    VK_CHECK(vkMapMemory(device_, ubo.memories[swapchain_index], 0, size, 0, &mapped), "map uniform memory");
    std::memcpy(mapped, data, size);
    vkUnmapMemory(device_, ubo.memories[swapchain_index]);
}

ModernPipeline* VkWrappedInstanceModern::find_pipeline(const std::string& name)
{
    auto found = pipelines_.find(name);
    return found != pipelines_.end() ? &found->second : nullptr;
}

ModernMeshGPU* VkWrappedInstanceModern::find_mesh(const std::string& name)
{
    auto found = meshes_.find(name);
    return found != meshes_.end() ? &found->second : nullptr;
}

void VkWrappedInstanceModern::bind_graphics_pipeline(VkCommandBuffer cmd, VkPipeline pipeline) const
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

void VkWrappedInstanceModern::cmd_set_viewport_scissor(VkCommandBuffer cmd) const
{
    const VkViewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(swapchain_extent_.width),
        .height = static_cast<float>(swapchain_extent_.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    const VkRect2D scissor{
        .offset = {0, 0},
        .extent = swapchain_extent_,
    };
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void VkWrappedInstanceModern::record_and_submit(uint32_t image_index)
{
    VkCommandBuffer cmd = command_buffers_[image_index];
    VK_CHECK(vkResetCommandBuffer(cmd, 0), "reset command buffer");

    VkCommandBufferBeginInfo begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info), "begin command buffer");

    transition_image_layout(cmd, swapchain_images_[image_index], VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

    std::array<VkRenderingAttachmentInfo, 2> attachments{};
    attachments[0] = VkRenderingAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = swapchain_image_views_[image_index],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {{0.02f, 0.02f, 0.05f, 1.0f}},
    };
    attachments[1] = VkRenderingAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = depth_image_views_[image_index],
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue = {{1.0f, 0}},
    };

    VkRenderingInfo rendering_info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {
            .offset = {0, 0},
            .extent = swapchain_extent_,
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachments[0],
        .pDepthAttachment = &attachments[1],
    };
    vkCmdBeginRendering(cmd, &rendering_info);

    const auto now = std::chrono::steady_clock::now();
    const float delta_seconds = std::chrono::duration<float>(now - last_frame_time_).count();
    last_frame_time_ = now;

    if (update_callback_) {
        update_callback_(image_index, cmd, delta_seconds);
    }

    vkCmdEndRendering(cmd);

    transition_image_layout(cmd, swapchain_images_[image_index],
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(vkEndCommandBuffer(cmd), "end command buffer");

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &image_available_semaphores_[current_frame_],
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &render_finished_semaphores_[current_frame_],
    };
    VK_CHECK(vkQueueSubmit(graphics_queue_, 1, &submit_info, in_flight_fences_[current_frame_]),
        "submit draw command buffer");

    VkPresentInfoKHR present_info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &render_finished_semaphores_[current_frame_],
        .swapchainCount = 1,
        .pSwapchains = &swapchain_,
        .pImageIndices = &image_index,
    };
    const VkResult present_result = vkQueuePresentKHR(present_queue_, &present_info);
    if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR || framebuffer_resized_) {
        framebuffer_resized_ = false;
        recreate_swapchain();
    }
    else if (present_result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swapchain image");
    }

    current_frame_ = (current_frame_ + 1) % kMaxFramesInFlight;
}

void VkWrappedInstanceModern::draw_frame()
{
    vkWaitForFences(device_, 1, &in_flight_fences_[current_frame_], VK_TRUE, UINT64_MAX);

    uint32_t image_index = 0;
    VkResult acquire_result = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
        image_available_semaphores_[current_frame_], VK_NULL_HANDLE, &image_index);

    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain();
        return;
    }
    if (acquire_result != VK_SUCCESS && acquire_result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swapchain image");
    }

    if (images_in_flight_[image_index] != VK_NULL_HANDLE) {
        vkWaitForFences(device_, 1, &images_in_flight_[image_index], VK_TRUE, UINT64_MAX);
    }
    images_in_flight_[image_index] = in_flight_fences_[current_frame_];

    vkResetFences(device_, 1, &in_flight_fences_[current_frame_]);
    record_and_submit(image_index);
}

void VkWrappedInstanceModern::mainloop()
{
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();
        draw_frame();
    }
    vkDeviceWaitIdle(device_);
}

void VkWrappedInstanceModern::destroy_swapchain_targets()
{
    if (device_ == VK_NULL_HANDLE) {
        return;
    }

    for (VkImageView view : depth_image_views_) {
        vkDestroyImageView(device_, view, nullptr);
    }
    for (VkImage image : depth_images_) {
        vkDestroyImage(device_, image, nullptr);
    }
    for (VkDeviceMemory memory : depth_memories_) {
        vkFreeMemory(device_, memory, nullptr);
    }
    depth_image_views_.clear();
    depth_images_.clear();
    depth_memories_.clear();

    for (VkImageView view : swapchain_image_views_) {
        vkDestroyImageView(device_, view, nullptr);
    }
    swapchain_image_views_.clear();
    swapchain_images_.clear();

    if (!command_buffers_.empty()) {
        vkFreeCommandBuffers(device_, command_pool_,
            static_cast<uint32_t>(command_buffers_.size()), command_buffers_.data());
        command_buffers_.clear();
    }

    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

void VkWrappedInstanceModern::recreate_swapchain()
{
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window_, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device_);
    destroy_swapchain_targets();
    create_swapchain_targets();
}

} // namespace vkkk
