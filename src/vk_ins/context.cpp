#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <map>
#include <ranges>
#include <stdexcept>
#include <vector>

#include <GLFW/glfw3.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

#include "vk_ins/context.hpp"

namespace vkkk
{

namespace
{

bool is_device_suitable(const vk::raii::PhysicalDevice& device, vk::raii::SurfaceKHR& surface,
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

uint32_t choose_min_image_count(const vk::SurfaceCapabilitiesKHR& surface_capabilities) {
    uint32_t min_image_count = surface_capabilities.minImageCount + 1;
    if (surface_capabilities.maxImageCount > 0 && min_image_count > surface_capabilities.maxImageCount) {
        min_image_count = surface_capabilities.maxImageCount;
    }
    return min_image_count;
}

vk::SurfaceFormatKHR choose_swap_surface_format(const std::vector<vk::SurfaceFormatKHR>& surface_formats) {
    const auto format_iter = std::ranges::find_if(surface_formats, [](const auto& surface_format) {
        return surface_format.format == vk::Format::eB8G8R8A8Srgb
            && surface_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
    });
    return format_iter != surface_formats.end() ? *format_iter : surface_formats.front();
}

vk::PresentModeKHR choose_present_mode(const std::vector<vk::PresentModeKHR>& present_modes) {
    const bool has_mailbox = std::ranges::any_of(present_modes, [](const auto& present_mode) {
        return present_mode == vk::PresentModeKHR::eMailbox;
    });
    return has_mailbox ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo;
}

vk::Extent2D choose_swap_extent(const vk::SurfaceCapabilitiesKHR& surface_capabilities, GLFWwindow* window) {
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

} // namespace

PipelineOption::PipelineOption() {
    vert_info = vk::PipelineVertexInputStateCreateInfo{};

    assembly_info = vk::PipelineInputAssemblyStateCreateInfo{};
    assembly_info.topology = vk::PrimitiveTopology::eTriangleList;
    assembly_info.primitiveRestartEnable = vk::False;

    viewport = vk::Viewport{0.f, 0.f, 800.f, 600.f, 0.f, 1.f};
    scissor = vk::Rect2D{{0, 0}, {800, 600}};
    viewport_info = vk::PipelineViewportStateCreateInfo{};
    viewport_info.viewportCount = 1;
    viewport_info.pViewports = &viewport;
    viewport_info.scissorCount = 1;
    viewport_info.pScissors = &scissor;

    raster_info = vk::PipelineRasterizationStateCreateInfo{};
    raster_info.depthClampEnable = vk::False;
    raster_info.rasterizerDiscardEnable = vk::False;
    raster_info.polygonMode = vk::PolygonMode::eFill;
    raster_info.cullMode = vk::CullModeFlagBits::eBack;
    raster_info.frontFace = vk::FrontFace::eCounterClockwise;
    raster_info.depthBiasEnable = vk::False;
    raster_info.lineWidth = 1.f;

    multisample_info = vk::PipelineMultisampleStateCreateInfo{};
    multisample_info.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisample_info.sampleShadingEnable = vk::False;

    depth_info = vk::PipelineDepthStencilStateCreateInfo{};
    depth_info.depthTestEnable = vk::True;
    depth_info.depthWriteEnable = vk::True;
    depth_info.depthCompareOp = vk::CompareOp::eLess;
    depth_info.depthBoundsTestEnable = vk::False;
    depth_info.stencilTestEnable = vk::False;

    blend_attachment_info = vk::PipelineColorBlendAttachmentState{};
    blend_attachment_info.blendEnable = vk::False;
    blend_attachment_info.colorWriteMask = vk::ColorComponentFlagBits::eR
        | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB
        | vk::ColorComponentFlagBits::eA;

    blend_info = vk::PipelineColorBlendStateCreateInfo{};
    blend_info.logicOpEnable = vk::False;
    blend_info.logicOp = vk::LogicOp::eCopy;
    blend_info.attachmentCount = 1;
    blend_info.pAttachments = &blend_attachment_info;

    dynamic_states = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    dynamic_info = vk::PipelineDynamicStateCreateInfo{};
    dynamic_info.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_info.pDynamicStates = dynamic_states.data();
}

void MeshGPU::sync(const Mesh& mesh, WrappedContext* ctx) {
    if (!mesh.loaded)
        throw std::runtime_error("cannot sync unloaded mesh");
    ctx->create_vertex_buffer(mesh.vbuf, vbuf, vbuf_memo, mesh.comp_size, mesh.vcnt);
    ctx->create_index_buffer(reinterpret_cast<const uint32_t*>(mesh.ibuf), ibuf, ibuf_memo, mesh.icnt * 3);
    icnt = mesh.icnt;
}

void MeshGPU::emit_draw_cmd(vk::CommandBuffer cmd_buf, vk::PipelineLayout ppl_layout,
    const vk::DescriptorSet* desc_set) const
{
    cmd_buf.bindVertexBuffers(0, *vbuf, {0});
    if (desc_set != nullptr) {
        cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, ppl_layout, 0, {*desc_set}, {});
    }
    cmd_buf.bindIndexBuffer(*ibuf, 0, vk::IndexType::eUint32);
    cmd_buf.drawIndexed(icnt * 3, 1, 0, 0, 0);
}

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

std::vector<const char*> WrappedContext::get_glfw_instance_extensions() {
    uint32_t count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);
    if (extensions == nullptr || count == 0) {
        throw std::runtime_error("failed to query GLFW instance extensions");
    }
    return {extensions, extensions + count};
}

GLFWwindow* WrappedContext::create_window(int width, int height, const char* title, bool resizable) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, resizable ? GLFW_TRUE : GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (window == nullptr) {
        throw std::runtime_error("failed to create GLFW window");
    }
    return window;
}

WrappedContext::WrappedContext(
    const char* app_name,
    uint32_t app_version,
    const char* engine_name,
    uint32_t api_version,
    bool enable_validation_layers,
    const std::vector<const char*>& extra_validation_layers,
    const std::vector<const char*>& extra_extensions,
    bool enable_debug_messenger)
    : enable_debug_messenger_(enable_debug_messenger)
{
    vk::ApplicationInfo app_info{app_name, app_version, engine_name, VK_MAKE_VERSION(1, 0, 0), api_version};

    std::vector<const char*> validation_layers;
    if (enable_validation_layers) {
        validation_layers.assign(default_validation_layers.begin(), default_validation_layers.end());
        validation_layers.insert(validation_layers.end(), extra_validation_layers.begin(), extra_validation_layers.end());
    }

    auto layer_props = context.enumerateInstanceLayerProperties();
    for (const auto* layer : validation_layers) {
        if (!std::ranges::any_of(layer_props, [layer](const auto& layer_prop) {
                return std::strcmp(layer_prop.layerName, layer) == 0;
            }))
        {
            throw std::runtime_error(std::string("unsupported validation layer: ") + layer);
        }
    }

    std::vector<const char*> instance_extensions = extra_extensions;
    auto extension_props = context.enumerateInstanceExtensionProperties();
    for (const auto* extension : instance_extensions) {
        if (!std::ranges::any_of(extension_props, [extension](const auto& extension_prop) {
                return std::strcmp(extension_prop.extensionName, extension) == 0;
            }))
        {
            throw std::runtime_error(std::string("unsupported instance extension: ") + extension);
        }
    }

    vk::DebugUtilsMessengerCreateInfoEXT debug_create_info{};
    const vk::DebugUtilsMessengerCreateInfoEXT* debug_create_info_ptr = nullptr;
    if (enable_validation_layers && enable_debug_messenger_) {
        vk::DebugUtilsMessageSeverityFlagsEXT severity_flags(
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
        vk::DebugUtilsMessageTypeFlagsEXT message_types(
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
            | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance);
        debug_create_info.messageSeverity = severity_flags;
        debug_create_info.messageType = message_types;
        debug_create_info.pfnUserCallback = &debug_callback;
        debug_create_info_ptr = &debug_create_info;
    }

    vk::InstanceCreateInfo instance_create_info{};
    instance_create_info.pNext = debug_create_info_ptr;
    instance_create_info.pApplicationInfo = &app_info;
    instance_create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
    instance_create_info.ppEnabledLayerNames = validation_layers.data();
    instance_create_info.enabledExtensionCount = static_cast<uint32_t>(instance_extensions.size());
    instance_create_info.ppEnabledExtensionNames = instance_extensions.data();
    instance = vk::raii::Instance(context, instance_create_info);

    if (enable_validation_layers && enable_debug_messenger_) {
        setup_debug_messenger();
    }
}

void WrappedContext::setup_debug_messenger() {
    vk::DebugUtilsMessageSeverityFlagsEXT severity_flags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                                          vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    vk::DebugUtilsMessageTypeFlagsEXT message_types(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                                    vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                                                    vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance);
    vk::DebugUtilsMessengerCreateInfoEXT debug_utils_messenger_create_info{};
    debug_utils_messenger_create_info.messageSeverity = severity_flags;
    debug_utils_messenger_create_info.messageType = message_types;
    debug_utils_messenger_create_info.pfnUserCallback = &debug_callback;
    debug_messenger = vk::raii::DebugUtilsMessengerEXT(instance, debug_utils_messenger_create_info);
}

void WrappedContext::transit_presentation_image_layout(
    vk::raii::CommandBuffer& cmd_buf,
    vk::Image img,
    vk::ImageLayout old_layout,
    vk::ImageLayout new_layout,
    vk::AccessFlags2 src_access_mask,
    vk::AccessFlags2 dst_access_mask,
    vk::PipelineStageFlags2 src_stage_mask,
    vk::PipelineStageFlags2 dst_stage_mask,
    vk::ImageAspectFlags aspect_mask) const
{
    vk::ImageMemoryBarrier2 barrier{};
    barrier.srcStageMask = src_stage_mask;
    barrier.srcAccessMask = src_access_mask;
    barrier.dstStageMask = dst_stage_mask;
    barrier.dstAccessMask = dst_access_mask;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = img;
    barrier.subresourceRange.aspectMask = aspect_mask;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    vk::DependencyInfo dependency_info{};
    dependency_info.imageMemoryBarrierCount = 1;
    dependency_info.pImageMemoryBarriers = &barrier;
    cmd_buf.pipelineBarrier2(dependency_info);
}

vk::Format WrappedContext::find_supported_format(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) const {
    for (const auto format : candidates) {
        vk::FormatProperties props = physical_device.getFormatProperties(format);
        if ((tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features)
            || (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features))
        {
            return format;
        }
    }
    throw std::runtime_error("failed to find supported format");
}

void WrappedContext::create_depth_resources() {
    vk::Format depth_format = find_depth_format();
    std::tie(depth_image, depth_memo) = create_vk_image(swapchain_extent.width, swapchain_extent.height, 1, vk::SampleCountFlagBits::e1, depth_format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::ImageCreateFlagBits::eNone, vk::MemoryPropertyFlagBits::eDeviceLocal);
    depth_view = create_vk_imageview(depth_image, depth_format, 1, vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);
}

void WrappedContext::create_swapchain() {
    // create the swapchain
    vk::SurfaceCapabilitiesKHR surface_capabilities = physical_device.getSurfaceCapabilitiesKHR(*surface);
    swapchain_extent = choose_swap_extent(surface_capabilities, window_);
    uint32_t min_image_count = choose_min_image_count(surface_capabilities);

    std::vector<vk::SurfaceFormatKHR> surface_formats = physical_device.getSurfaceFormatsKHR(*surface);
    swapchain_surface_format = choose_swap_surface_format(surface_formats);

    std::vector<vk::PresentModeKHR> present_modes = physical_device.getSurfacePresentModesKHR(*surface);
    vk::PresentModeKHR present_mode = choose_present_mode(present_modes);

    vk::SwapchainCreateInfoKHR swapchain_create_info{};
    swapchain_create_info.surface = *surface;
    swapchain_create_info.minImageCount = min_image_count;
    swapchain_create_info.imageFormat = swapchain_surface_format.format;
    swapchain_create_info.imageColorSpace = swapchain_surface_format.colorSpace;
    swapchain_create_info.imageExtent = swapchain_extent;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    swapchain_create_info.imageSharingMode = vk::SharingMode::eExclusive;
    swapchain_create_info.preTransform = surface_capabilities.currentTransform;
    swapchain_create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    swapchain_create_info.presentMode = present_mode;
    swapchain_create_info.clipped = VK_TRUE;
    swapchain = vk::raii::SwapchainKHR(device, swapchain_create_info);
    swapchain_images = swapchain.getImages();
}

void WrappedContext::create_imageviews() {
    swapchain_image_views.clear();
    swapchain_image_views.reserve(swapchain_images.size());
    for (const auto& image : swapchain_images) {
        swapchain_image_views.emplace_back(create_vk_imageview(image, swapchain_surface_format.format));
    }
}

uint32_t WrappedContext::find_memory_type(uint32_t type_filter, vk::MemoryPropertyFlags properties) const {
    vk::PhysicalDeviceMemoryProperties mem_props = physical_device.getMemoryProperties();
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_filter & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type");
}

std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> WrappedContext::create_buffer(vk::DeviceSize size,
    vk::BufferUsageFlags usage,
    vk::MemoryPropertyFlags properties) const
{
    vk::BufferCreateInfo buffer_create_info{};
    buffer_create_info.size = size;
    buffer_create_info.usage = usage;
    buffer_create_info.sharingMode = vk::SharingMode::eExclusive;
    vk::raii::Buffer buffer = vk::raii::Buffer(device, buffer_create_info);
    vk::MemoryRequirements mem_reqs = buffer.getMemoryRequirements();
    vk::MemoryAllocateInfo alloc_info{};
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits, properties);
    vk::raii::DeviceMemory memo = vk::raii::DeviceMemory(device, alloc_info);
    buffer.bindMemory(*memo, 0);
    return std::make_pair(std::move(buffer), std::move(memo));
}

vk::raii::CommandBuffer WrappedContext::begin_single_commands() const {
    vk::CommandBufferAllocateInfo alloc_info{};
    alloc_info.commandPool = *command_pool;
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    alloc_info.commandBufferCount = 1;
    vk::raii::CommandBuffers command_buffers(device, alloc_info);
    vk::raii::CommandBuffer command_buffer = std::move(command_buffers[0]);

    vk::CommandBufferBeginInfo begin_info{};
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    command_buffer.begin(begin_info);
    return std::move(command_buffer);
}

void WrappedContext::end_single_commands(vk::raii::CommandBuffer&& cmd_buf) const {
    cmd_buf.end();
    vk::CommandBuffer raw_cmd = *cmd_buf;
    vk::SubmitInfo submit_info{};
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &raw_cmd;
    queue.submit(submit_info, nullptr);
    queue.waitIdle();
}

void WrappedContext::copy_buffer(vk::raii::Buffer& src, vk::raii::Buffer& dst, vk::DeviceSize size) const {
    vk::raii::CommandBuffer cmd_buf = begin_single_commands();
    cmd_buf.copyBuffer(*src, *dst, vk::BufferCopy(0, 0, size));
    end_single_commands(std::move(cmd_buf));
}

std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> WrappedContext::load_into_staging_buffer(void* data, uint32_t size) const {
    auto [staging_buf, staging_memo] = create_buffer(size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    void* mapped = device.mapMemory(*staging_memo, 0, size);
    std::memcpy(mapped, data, size);
    device.unmapMemory(*staging_memo);
    return std::make_pair(std::move(staging_buf), std::move(staging_memo));
}

void WrappedContext::init(GLFWwindow* window) {
    window_ = window;

    VkSurfaceKHR raw_surface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(static_cast<VkInstance>(*instance), window_, nullptr, &raw_surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface");
    }
    surface = vk::raii::SurfaceKHR(instance, raw_surface);

    std::vector<vk::raii::PhysicalDevice> devices = instance.enumeratePhysicalDevices();
    auto const dev_iter = std::ranges::find_if(devices, [&](const auto& device) {
        return is_device_suitable(device, surface, required_extensions);
    });
    if (dev_iter == devices.end()) {
        throw std::runtime_error("no suitable Vulkan 1.3 device with dynamic rendering support");
    }
    physical_device = *dev_iter;

    const std::vector<vk::QueueFamilyProperties> queue_family_properties = physical_device.getQueueFamilyProperties();
    for (uint32_t i = 0; i < queue_family_properties.size(); ++i) {
        if ((queue_family_properties[i].queueFlags & vk::QueueFlagBits::eGraphics) && physical_device.getSurfaceSupportKHR(i, *surface)) {
            queue_idx = i;
            break;
        }
    }
    if (queue_idx == ~0u) {
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

    create_swapchain();
    create_imageviews();

    // create the command pool
    // TODO: eResetCommandBuffer is a mode the command pool kinda persists
    // there's also another flag eTransient that is more like a one-time use pool
    // maybe needed in the future. Might need more methods and fields to 
    // manage the command pool.
    vk::CommandPoolCreateInfo command_pool_create_info{
        .queueFamilyIndex = queue_idx,
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer
    };
    command_pool = vk::raii::CommandPool(device, command_pool_create_info);

    // create the depth resources
    create_depth_resources();

    image_available_semaphores.clear();
    render_finished_semaphores.clear();
    in_flight_fences.clear();
    images_in_flight.assign(swapchain_images.size(), VK_NULL_HANDLE);

    for (size_t i = 0; i < max_frames_in_flight; ++i) {
        image_available_semaphores.emplace_back(device, vk::SemaphoreCreateInfo{});
        render_finished_semaphores.emplace_back(device, vk::SemaphoreCreateInfo{});
        in_flight_fences.emplace_back(device, vk::FenceCreateInfo{
            .flags = vk::FenceCreateFlagBits::eSignaled
        });
    }

    command_buffers = vk::raii::CommandBuffers(device, vk::CommandBufferAllocateInfo{
        .commandPool = command_pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = static_cast<uint32_t>(swapchain_images.size())
    });
}

static std::vector<vk::VertexInputBindingDescription> gen_binding_desc(const std::vector<VERT_COMP>& comps, bool interleaved) {
    // a vertex binding itself being interleaved or not is irrelavent to others
    // the mesh format can be quite flexible, for example, vertices being a single buf,
    // uv and color interleaved being another buf. Each binding desc is related to its
    // attr desc.
    // Here we assume the mesh being either all interleaved or all separated.
    std::vector<vk::VertexInputBindingDescription> binding_descriptions;
    if (interleaved) {
        binding_descriptions.emplace_back(0, get_mesh_component_size(comps) * sizeof(float), vk::VertexInputRate::eVertex);
    }
    else {
        for (const auto& comp : comps) {
            binding_descriptions.emplace_back(0, comp_sizes[comp] * sizeof(float), vk::VertexInputRate::eVertex);
        }
    }
    return binding_descriptions;
}

bool WrappedContext::create_pipeline(const std::string& name,
    const ShaderModulePack& shader_module_pack,
    const PipelineOption& option,
    const std::vector<VERT_COMP>& comps,
    bool interleaved)
{
    if (pipelines.find(name) != pipelines.end()) {
        std::cout << "Pipeline " << name << " already exists" << std::endl;
        return false;
    }

    std::vector<vk::VertexInputBindingDescription> input_binding_descs;
    std::vector<vk::VertexInputAttributeDescription> input_attr_descs;
    std::vector<vk::DescriptorSetLayoutBinding> descriptor_layouts;
    std::map<uint32_t, std::string> ubo_binding_to_name;
    std::map<uint32_t, std::string> tex_binding_to_name;

    std::vector<vk::raii::ShaderModule> shader_modules;
    std::vector<vk::PipelineShaderStageCreateInfo> shader_stage_infos;
    shader_modules.reserve(shader_module_pack.modules.size());
    shader_stage_infos.reserve(shader_module_pack.modules.size());

    for (const auto& [stage, module] : shader_module_pack.modules) {
        vk::ShaderModuleCreateInfo shader_module_create_info{
            .codeSize = module.spirv_code.size() * sizeof(uint32_t),
            .pCode = module.spirv_code.data()
        };
        shader_modules.emplace_back(device, shader_module_create_info);
        shader_stage_infos.push_back(vk::PipelineShaderStageCreateInfo{
            .stage = stage,
            .module = *shader_modules.back(),
            .pName = "main"
        });

        if (stage == vk::ShaderStageFlagBits::eVertex) {
            input_binding_descs = gen_binding_desc(comps, interleaved);

            uint32_t offset = 0;
            int binding_idx = 0;
            for (const auto& [attr_loc, glsl_type, attr_name] : module.attr_infos) {
                (void)attr_name;
                if (interleaved) {
                    input_attr_descs.push_back(vk::VertexInputAttributeDescription{
                        .location = attr_loc,
                        .binding = 0,
                        .format = static_cast<vk::Format>(glsl_type_macro[glsl_type]),
                        .offset = offset
                    });
                    offset += glsl_type_sizes[glsl_type];
                }
                else {
                    input_attr_descs.push_back(vk::VertexInputAttributeDescription{
                        .location = attr_loc,
                        .binding = static_cast<uint32_t>(binding_idx),
                        .format = static_cast<vk::Format>(glsl_type_macro[glsl_type]),
                        .offset = 0
                    });
                }
                ++binding_idx;
            }
        }

        for (const auto& [ubo_name, ubo_info] : module.buf_infos) {
            const auto& [struct_size, array_size, binding] = ubo_info;
            const auto ppl_ubo_name = name + ":" + ubo_name;
            add_ubo(ppl_ubo_name, binding, struct_size, array_size);
            ubo_binding_to_name[binding] = ppl_ubo_name;

            descriptor_layouts.push_back(vk::DescriptorSetLayoutBinding{
                .binding = binding,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .descriptorCount = array_size,
                .stageFlags = stage
            });
        }

        for (const auto& [tex_name, tex_binding] : module.img_infos) {
            const auto ppl_tex_name = name + ":" + tex_name;
            uint32_t descriptor_count = 1;
            if (textures.find(ppl_tex_name) == textures.end()) {
                const auto tex_path_info = module.tex_img_pairs.find(tex_name);
                if (tex_path_info == module.tex_img_pairs.end()) {
                    std::cout << "No texture assigned for sampler " << tex_name << std::endl;
                    continue;
                }

                const auto& [path, is_cubemap] = tex_path_info->second;
                descriptor_count = is_cubemap ? 6u : 1u;
                if (!is_cubemap) {
                    if (!add_texture(ppl_tex_name, tex_binding, path)) {
                        continue;
                    }
                }
                else if (!add_cubemap(ppl_tex_name, tex_binding, path)) {
                    continue;
                }
            }
            else {
                descriptor_count = static_cast<uint32_t>(textures.at(ppl_tex_name).vecsize);
            }
            tex_binding_to_name[tex_binding] = ppl_tex_name;

            descriptor_layouts.push_back(vk::DescriptorSetLayoutBinding{
                .binding = tex_binding,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .descriptorCount = descriptor_count,
                .stageFlags = stage
            });
        }
    }

    PipelineOption local_option = option;
    local_option.vert_info.vertexBindingDescriptionCount = static_cast<uint32_t>(input_binding_descs.size());
    local_option.vert_info.pVertexBindingDescriptions = input_binding_descs.data();
    local_option.vert_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(input_attr_descs.size());
    local_option.vert_info.pVertexAttributeDescriptions = input_attr_descs.data();

    vk::raii::DescriptorSetLayout descriptor_set_layout{nullptr};
    if (!descriptor_layouts.empty()) {
        descriptor_set_layout = vk::raii::DescriptorSetLayout(device, vk::DescriptorSetLayoutCreateInfo{
            .bindingCount = static_cast<uint32_t>(descriptor_layouts.size()),
            .pBindings = descriptor_layouts.data()
        });
    }

    vk::PipelineLayoutCreateInfo pipeline_layout_info{
        .setLayoutCount = descriptor_set_layout ? 1u : 0u,
        .pSetLayouts = descriptor_set_layout ? &*descriptor_set_layout : nullptr
    };
    vk::raii::PipelineLayout pipeline_layout(device, pipeline_layout_info);

    const vk::Format depth_format = find_depth_format();
    vk::PipelineRenderingCreateInfo rendering_create_info{
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &swapchain_surface_format.format,
        .depthAttachmentFormat = depth_format
    };
    vk::GraphicsPipelineCreateInfo graphics_pipeline_create_info{
        .stageCount = static_cast<uint32_t>(shader_stage_infos.size()),
        .pStages = shader_stage_infos.data(),
        .pVertexInputState = &local_option.vert_info,
        .pInputAssemblyState = &local_option.assembly_info,
        .pViewportState = &local_option.viewport_info,
        .pRasterizationState = &local_option.raster_info,
        .pMultisampleState = &local_option.multisample_info,
        .pDepthStencilState = &local_option.depth_info,
        .pColorBlendState = &local_option.blend_info,
        .pDynamicState = &local_option.dynamic_info,
        .layout = *pipeline_layout,
        .renderPass = nullptr
    };
    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipeline_create_info{
        graphics_pipeline_create_info,
        rendering_create_info
    };
    vk::raii::Pipeline vk_pipeline(device, nullptr, pipeline_create_info.get<vk::GraphicsPipelineCreateInfo>());

    Pipeline pipeline;
    pipeline.vk_pipeline = std::move(vk_pipeline);
    pipeline.vk_pipeline_layout = std::move(pipeline_layout);
    pipeline.descriptor_set_layout = std::move(descriptor_set_layout);

    const uint32_t swapchain_cnt = static_cast<uint32_t>(swapchain_images.size());
    uint32_t uniform_desc_count = 0;
    for (const auto& [binding, ubo_name] : ubo_binding_to_name) {
        (void)binding;
        uniform_desc_count += static_cast<uint32_t>(require_ubo(ubo_name).vecsize);
    }
    uint32_t image_desc_count = 0;
    for (const auto& [binding, tex_name] : tex_binding_to_name) {
        (void)binding;
        const auto tex_found = textures.find(tex_name);
        if (tex_found != textures.end()) {
            image_desc_count += static_cast<uint32_t>(tex_found->second.vecsize);
        }
    }

    std::vector<vk::DescriptorPoolSize> pool_sizes;
    if (uniform_desc_count > 0) {
        pool_sizes.push_back(vk::DescriptorPoolSize{
            .type = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = uniform_desc_count * swapchain_cnt
        });
    }
    if (image_desc_count > 0) {
        pool_sizes.push_back(vk::DescriptorPoolSize{
            .type = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = image_desc_count * swapchain_cnt
        });
    }

    if (!pool_sizes.empty()) {
        pipeline.descriptor_pool = vk::raii::DescriptorPool(device, vk::DescriptorPoolCreateInfo{
            .maxSets = swapchain_cnt,
            .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
            .pPoolSizes = pool_sizes.data()
        });

        std::vector<vk::DescriptorSetLayout> set_layouts(swapchain_cnt, *pipeline.descriptor_set_layout);
        pipeline.descriptor_sets = vk::raii::DescriptorSets(device, vk::DescriptorSetAllocateInfo{
            .descriptorPool = *pipeline.descriptor_pool,
            .descriptorSetCount = swapchain_cnt,
            .pSetLayouts = set_layouts.data()
        });

        for (uint32_t i = 0; i < swapchain_cnt; ++i) {
            std::vector<vk::DescriptorBufferInfo> buffer_infos;
            std::vector<vk::DescriptorImageInfo> image_infos;
            std::vector<vk::WriteDescriptorSet> writes;
            buffer_infos.reserve(ubo_binding_to_name.size());
            image_infos.reserve(tex_binding_to_name.size());
            writes.reserve(ubo_binding_to_name.size() + tex_binding_to_name.size());

            for (const auto& [binding, ubo_name] : ubo_binding_to_name) {
                auto& ubo = require_ubo(ubo_name);
                buffer_infos.push_back(vk::DescriptorBufferInfo{
                    .buffer = *ubo.gpu_bufs[i],
                    .offset = 0,
                    .range = ubo.size * ubo.vecsize
                });
                writes.push_back(vk::WriteDescriptorSet{
                    .dstSet = *pipeline.descriptor_sets[i],
                    .dstBinding = binding,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eUniformBuffer,
                    .pBufferInfo = &buffer_infos.back()
                });
            }

            for (const auto& [binding, tex_name] : tex_binding_to_name) {
                const auto tex_found = textures.find(tex_name);
                if (tex_found == textures.end()) {
                    continue;
                }
                image_infos.push_back(tex_found->second.descriptor);
                writes.push_back(vk::WriteDescriptorSet{
                    .dstSet = *pipeline.descriptor_sets[i],
                    .dstBinding = binding,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                    .pImageInfo = &image_infos.back()
                });
            }

            if (!writes.empty()) {
                device.updateDescriptorSets(writes, {});
            }
        }
    }

    pipelines.emplace(name, std::move(pipeline));
    return true;
}

void WrappedContext::record_cmds(uint32_t image_index,
    const std::function<void(vk::raii::CommandBuffer&, uint32_t)>& emit_func)
{
    auto& cmd_buf = command_buffers[image_index];
    cmd_buf.reset({});
    cmd_buf.begin({});

    transit_presentation_image_layout(
        cmd_buf,
        swapchain_images[image_index],
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        {},
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::ImageAspectFlagBits::eColor);

    transit_presentation_image_layout(
        cmd_buf,
        static_cast<vk::Image>(*depth_image),
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eDepthStencilAttachmentOptimal,
        {},
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::ImageAspectFlagBits::eDepth);

    vk::ClearValue clear_value = vk::ClearColorValue(std::array<float, 4>{0.f, 0.f, 0.f, 1.f});
    vk::ClearValue depth_clear_value = vk::ClearDepthStencilValue(1.f, 0);
    vk::RenderingAttachmentInfo color_attachment_info{
        .imageView = *swapchain_image_views[image_index],
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = clear_value
    };
    vk::RenderingAttachmentInfo depth_attachment_info{
        .imageView = *depth_view,
        .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = depth_clear_value
    };
    vk::RenderingInfo rendering_info{
        .renderArea = {.offset = {0, 0}, .extent = swapchain_extent},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_info,
        .pDepthAttachment = &depth_attachment_info
    };
    cmd_buf.beginRendering(rendering_info);
    cmd_buf.setViewport(0, vk::Viewport{
        0.f, 0.f,
        static_cast<float>(swapchain_extent.width),
        static_cast<float>(swapchain_extent.height),
        0.f, 1.f
    });
    cmd_buf.setScissor(0, vk::Rect2D{{0, 0}, swapchain_extent});

    emit_func(cmd_buf, image_index);

    cmd_buf.endRendering();
    transit_presentation_image_layout(
        cmd_buf,
        swapchain_images[image_index],
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        {},
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eBottomOfPipe,
        vk::ImageAspectFlagBits::eColor);
    cmd_buf.end();
}

void WrappedContext::draw_frame() {
    device.waitForFences(*in_flight_fences[current_frame], vk::True, UINT64_MAX);

    auto [result, image_index] = swapchain.acquireNextImage(UINT64_MAX, *image_available_semaphores[current_frame], nullptr);
    if (result == vk::Result::eErrorOutOfDateKHR) {
        recreate_swapchain();
        return;
    }
    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
        throw std::runtime_error("failed to acquire swapchain image");
    }

    if (images_in_flight[image_index] != VK_NULL_HANDLE) {
        device.waitForFences(images_in_flight[image_index], vk::True, UINT64_MAX);
    }
    images_in_flight[image_index] = *in_flight_fences[current_frame];

    const auto now = std::chrono::steady_clock::now();
    const float dt = std::chrono::duration<float>(now - last_frame_time_).count();
    last_frame_time_ = now;

    if (update_cbk_) {
        update_cbk_(image_index, dt);
    }

    device.resetFences(*in_flight_fences[current_frame]);

    vk::PipelineStageFlags wait_stage_mask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    vk::SubmitInfo submit_info{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*image_available_semaphores[current_frame],
        .pWaitDstStageMask = &wait_stage_mask,
        .commandBufferCount = 1,
        .pCommandBuffers = &*command_buffers[image_index],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &*render_finished_semaphores[current_frame]
    };
    queue.submit(submit_info, *in_flight_fences[current_frame]);

    vk::SwapchainKHR swapchains[] = {*swapchain};
    vk::PresentInfoKHR present_info{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*render_finished_semaphores[current_frame],
        .swapchainCount = 1,
        .pSwapchains = swapchains,
        .pImageIndices = &image_index
    };
    result = queue.presentKHR(present_info);
    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || frame_buffer_resized) {
        frame_buffer_resized = false;
        recreate_swapchain();
    }

    current_frame = (current_frame + 1) % max_frames_in_flight;
}

void WrappedContext::recreate_swapchain() {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window_, &width, &height);
        glfwWaitEvents();
    }
    device.waitIdle();

    swapchain_image_views.clear();
    depth_view = nullptr;
    depth_image = nullptr;
    depth_memo = nullptr;
    swapchain = nullptr;
    images_in_flight.assign(swapchain_images.size(), VK_NULL_HANDLE);

    create_swapchain();
    create_imageviews();
    create_depth_resources();

    command_buffers = vk::raii::CommandBuffers(device, vk::CommandBufferAllocateInfo{
        .commandPool = command_pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = static_cast<uint32_t>(swapchain_images.size())
    });
}

bool WrappedContext::add_ubo(const std::string& name, uint32_t binding,
    uint32_t size, uint32_t vecsize)
{
    if (ubos.find(name) != ubos.end()) {
        std::cout << "UBO " << name << " already exists" << std::endl;
        return false;
    }

    UBO ubo{.size = size, .vecsize = vecsize, .binding = binding};
    ubo.cpu_buf = std::make_shared<char[]>(size * vecsize);
    ubo.gpu_bufs.resize(swapchain_images.size());
    ubo.memos.resize(swapchain_images.size());
    ubo.descriptors.resize(swapchain_images.size());

    for (size_t i = 0; i < swapchain_images.size(); ++i) {
        std::tie(ubo.gpu_bufs[i], ubo.memos[i]) = create_buffer(
            size * vecsize,
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        ubo.descriptors[i] = vk::DescriptorBufferInfo{*ubo.gpu_bufs[i], 0, size * vecsize};
    }

    ubos.emplace(name, std::move(ubo));
    return true;
}

void WrappedContext::sync_uniform(const vk::raii::DeviceMemory& memo, const void* data, uint32_t size) const {
    void* mapped = memo.mapMemory(0, size);
    std::memcpy(mapped, data, size);
    memo.unmapMemory();
}

UBO& WrappedContext::require_ubo(const std::string& full_name) {
    const auto found = ubos.find(full_name);
    if (found == ubos.end()) {
        throw std::runtime_error("ubo not found: " + full_name);
    }
    return found->second;
}

bool WrappedContext::load_mesh(const std::string& name, const Mesh& mesh) {
    MeshGPU gpu{};
    gpu.sync(mesh, this);
    meshes.emplace(name, std::move(gpu));
    return true;
}

bool WrappedContext::add_texture(const std::string& name, const uint32_t binding,
    const fs::path& path) {
    if (textures.find(name) != textures.end()) {
        std::cout << "Texture " << name << " already exists" << std::endl;
        return false;
    }

    Texture tex{.binding = binding, .vecsize = 1};
    fs::path abs_path = path;
    if (path.is_relative())
        abs_path = fs::absolute(path);
    if (!fs::exists(abs_path)) {
        std::cout << "path for texture " << name << "does not exist" << std::endl;
        return false;
    }

    // load image with oiio
    OIIO::ImageBuf oiio_buf(abs_path.string().c_str());
    if (!oiio_buf.init_spec(oiio_buf.name(), 0, 0)) {
        std::cout << "[OIIO] Texture spec initialization for " << name << " failed"
            << std::endl;
        return false;
    }

    int ch_ords[] = {0, 1, 2, -1};
    float ch_vals[] = {0, 0, 0, 1.f};
    std::string ch_names[] = {"R", "G", "B", "A"};
    OIIO::ImageBuf with_alpha_buf = OIIO::ImageBufAlgo::channels(oiio_buf, 4, ch_ords,
        ch_vals, ch_names);
    
    auto spec = with_alpha_buf.spec();
    // Default to R8G8B8A8
    vk::DeviceSize image_size = spec.width * spec.height * sizeof(float);
    std::vector<float> pixels;
    pixels.resize(spec.width * spec.height);
    with_alpha_buf.get_pixels(OIIO::ROI::All(), OIIO::TypeDesc::UINT8, pixels.data());

    // load image
    auto [staging_buf, staging_memo] = load_into_staging_buffer(pixels.data(), image_size);

    std::tie(tex.image, tex.memo) = create_vk_image(spec.width, spec.height, 1, vk::SampleCountFlagBits::e1,
        vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
        vk::ImageCreateFlags::eNone, vk::MemoryPropertyFlagBits::eDeviceLocal);

    vk::raii::CommandBuffer cmd_buf = begin_single_commands();
    transit_image_layout(cmd_buf, tex.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    copy_buffer_to_image(cmd_buf, staging_buf, tex.image, spec.width, spec.height);
    transit_image_layout(cmd_buf, tex.image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
    end_single_commands(std::move(cmd_buf));

    tex.view = create_vk_imageview(tex.image, vk::Format::eR8G8B8A8Srgb);

    // create image sampler
    tex.sampler = create_vk_sampler();

    tex.layout = vk::ImageLayout::eShaderReadOnlyOptimal;
    tex.descriptor = vk::DescriptorImageInfo{*tex.sampler, *tex.view, tex.layout};

    textures.emplace(name, std::move(tex));
    return true;
}

bool WrappedContext::add_cubemap(const std::string& name, const uint32_t binding,
    const fs::path& path)
{
    if (textures.find(name) != textures.end()) {
        std::cout << "Texture " << name << " already exists" << std::endl;
        return false;
    }

    Texture tex{.binding = binding, .vecsize = 6};

    fs::path abs_path = path;
    if (path.is_relative()) {
        abs_path = fs::absolute(path);
    }
    if (!fs::exists(abs_path)) {
        std::cout << "path for cubemap " << name << " does not exist" << std::endl;
        return false;
    }

    OIIO::ImageBuf oiio_buf(abs_path.string().c_str());
    if (!oiio_buf.init_spec(oiio_buf.name(), 0, 0)) {
        std::cout << "[OIIO] Cubemap spec initialization for " << name << " failed" << std::endl;
        return false;
    }

    oiio_buf.read();
    const auto spec = oiio_buf.spec();
    const uint32_t face_size = spec.width / 4;

    const auto top_buf = OIIO::ImageBufAlgo::cut(oiio_buf, OIIO::ROI(face_size, face_size * 2, 0, face_size));
    const auto bottom_buf = OIIO::ImageBufAlgo::cut(oiio_buf, OIIO::ROI(face_size, face_size * 2, face_size * 2, face_size * 3));
    const auto left_buf = OIIO::ImageBufAlgo::cut(oiio_buf, OIIO::ROI(0, face_size, face_size, face_size * 2));
    const auto right_buf = OIIO::ImageBufAlgo::cut(oiio_buf, OIIO::ROI(face_size * 2, face_size * 3, face_size, face_size * 2));
    const auto front_buf = OIIO::ImageBufAlgo::cut(oiio_buf, OIIO::ROI(face_size, face_size * 2, face_size, face_size * 2));
    const auto back_buf = OIIO::ImageBufAlgo::cut(oiio_buf, OIIO::ROI(face_size * 3, face_size * 4, face_size, face_size * 2));
    const std::array<OIIO::ImageBuf, 6> face_bufs = {right_buf, left_buf, top_buf, bottom_buf, front_buf, back_buf};

    std::vector<float> pixel_pool;
    pixel_pool.reserve(face_size * face_size * 6);
    const vk::DeviceSize image_size = static_cast<vk::DeviceSize>(face_size) * face_size * sizeof(float) * 6;

    int ch_ords[] = {0, 1, 2, -1};
    float ch_vals[] = {0, 0, 0, 1.f};
    std::string ch_names[] = {"R", "G", "B", "A"};
    for (const auto& face_buf : face_bufs) {
        const OIIO::ImageBuf with_alpha_buf = OIIO::ImageBufAlgo::channels(face_buf, 4, ch_ords,
            ch_vals, ch_names);

        std::vector<float> pixels(face_size * face_size);
        with_alpha_buf.get_pixels(OIIO::ROI::All(), OIIO::TypeDesc::UINT8, pixels.data());
        pixel_pool.insert(pixel_pool.end(), std::make_move_iterator(pixels.begin()), std::make_move_iterator(pixels.end()));
    }

    auto [staging_buf, staging_memo] = load_into_staging_buffer(pixel_pool.data(), static_cast<uint32_t>(image_size));

    // create image
    std::tie(tex.image, tex.memo) = create_vk_image(face_size, face_size, 6, vk::SampleCountFlagBits::e1,
        vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
        vk::ImageCreateFlagBits::eCubeCompatible, vk::MemoryPropertyFlagBits::eDeviceLocal);

    vk::raii::CommandBuffer cmd_buf = begin_single_commands();

    transit_image_layout(cmd_buf, tex.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, 6);

    std::vector<vk::BufferImageCopy> regions;
    regions.reserve(6);
    vk::DeviceSize offset = 0;
    const vk::DeviceSize face_bytes = static_cast<vk::DeviceSize>(face_size) * face_size * sizeof(float);
    for (uint32_t layer = 0; layer < 6; ++layer) {
        vk::BufferImageCopy region{};
        region.bufferOffset = offset;
        region.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, layer, 1};
        region.imageOffset = vk::Offset3D{0, 0, 0};
        region.imageExtent = vk::Extent3D{face_size, face_size, 1};
        regions.push_back(region);
        offset += face_bytes;
    }
    cmd_buf.copyBufferToImage(*staging_buf, *tex.image, vk::ImageLayout::eTransferDstOptimal, {regions});

    transit_image_layout(cmd_buf, tex.image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, 6);
    end_single_commands(std::move(cmd_buf));

    // create image view
    tex.view = create_vk_imageview(tex.image, vk::Format::eR8G8B8A8Srgb, 6);

    // create sampler
    tex.sampler = create_vk_sampler();

    tex.layout = vk::ImageLayout::eShaderReadOnlyOptimal;
    tex.descriptor = vk::DescriptorImageInfo{*tex.sampler, *tex.view, tex.layout};

    textures.emplace(name, std::move(tex));
    return true;
}

void WrappedContext::transit_image_layout(vk::raii::CommandBuffer& cmd_buf, const vk::raii::Image& img, vk::ImageLayout old_layout, vk::ImageLayout new_layout, const uint32_t layer_count) const {
    vk::ImageMemoryBarrier barrier{
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image = img,
        .subresourceRange = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .levelCount = 1,
            .layerCount = layer_count
        }
    };
    vk::PipelineStageFlags src_stage;
    vk::PipelineStageFlags dst_stage;

    if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits2::eTransferWrite;
        src_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        dst_stage = vk::PipelineStageFlagBits::eTransfer;
    }
    else if (old_layout == vk::ImageLayout::eTransferDstOptimal && new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
        src_stage = vk::PipelineStageFlagBits::eTransfer;
        dst_stage = vk::PipelineStageFlagBits::eFragmentShader;
    }
    else {
        throw std::runtime_error("unsupported image layout transition");
    }
    cmd_buf.pipelineBarrier(src_stage, dst_stage, {}, {}, {}, barrier);
}

void WrappedContext::copy_buffer_to_image(vk::raii::CommandBuffer& cmd_buf, const vk::raii::Buffer& buf, const vk::raii::Image& img, uint32_t width, uint32_t height) const {
    vk::BufferImageCopy region{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {width, height, 1}
    };
    cmd_buf.copyBufferToImage(buf, img, vk::ImageLayout::eTransferDstOptimal, region);
}

std::pair<vk::raii::Image, vk::raii::DeviceMemory> WrappedContext::create_vk_image(uint32_t width, uint32_t height, uint32_t layers, vk::SampleCountFlagBits samples, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::ImageCreateFlags flags, vk::MemoryPropertyFlags properties) const {
    vk::ImageCreateInfo image_create_info{
        .imageType = vk::ImageType::e2D,
        .format = format,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = layers,
        .samples = samples,
        .tiling = tiling,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive,
        .flags = flags
    };
    vk::raii::Image image = vk::raii::Image(device, image_create_info);
    vk::MemoryRequirements mem_reqs = image.getMemoryRequirements();
    vk::MemoryAllocateInfo alloc_info{
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits, properties)
    };
    vk::raii::DeviceMemory memo = vk::raii::DeviceMemory(device, alloc_info);
    image.bindMemory(memo, 0);
    return {std::move(image), std::move(memo)};
}

vk::raii::ImageView WrappedContext::create_vk_imageview(vk::Image img, vk::Format format, vk::ImageAspectFlags aspect_mask) const {
    vk::ImageViewCreateInfo view_info{
        .image = img,
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .subresourceRange = {.aspectMask = aspect_mask, .levelCount = 1, .layerCount = 1}
    };
    return vk::raii::ImageView(device, view_info);
}

vk::raii::ImageView WrappedContext::create_vk_imageview(const vk::raii::Image& img, vk::Format format, uint32_t layer_count,
    vk::ImageAspectFlags aspect_mask) const
{
    vk::ImageViewCreateInfo view_info{
        .image = *img,
        .viewType = layer_count == 1 ? vk::ImageViewType::e2D : vk::ImageViewType::eCube,
        .format = format,
        .subresourceRange = {.aspectMask = aspect_mask, .levelCount = 1, .layerCount = layer_count}
    };
    return vk::raii::ImageView(device, view_info);
}

vk::raii::Sampler WrappedContext::create_vk_sampler(vk::Filter mag_filter, vk::Filter min_filter, vk::SamplerMipmapMode mipmap_mode, vk::SamplerAddressMode address_mode, bool anisotropy_enable, bool compare_enable, vk::CompareOp compare_op) const {
    vk::PhysicalDeviceProperties props = physical_device.getProperties();
    vk::SamplerCreateInfo sampler_info{
        .magFilter = mag_filter,
        .minFilter = min_filter,
        .mipmapMode = mipmap_mode,
        .addressModeU = address_mode,
        .addressModeV = address_mode,
        .addressModeW = address_mode,
        .mipLodBias = 0.f,
        .anisotropyEnable = anisotropy_enable,
        .maxAnisotropy = props.limits.maxSamplerAnisotropy,
        .compareEnable = compare_enable,
        .compareOp = compare_op
    };
    return vk::raii::Sampler(device, sampler_info);
}

}