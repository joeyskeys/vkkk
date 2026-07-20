#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <map>
#include <ranges>
#include <stdexcept>
#include <vector>

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

#include "asset_mgr/light_mgr.h"
#include "vk_ins/context.hpp"

namespace vkkk
{

namespace {

bool resolve_ubo_type(const std::string& reflected_name, UBOType& out_type) {
    // Match GLSL block/type names from reflection (get_name(base_type_id)).
    if (reflected_name == "CameraUBO"
        || reflected_name == "UniformBufferObject"
        || reflected_name == "Camera")
    {
        out_type = UBOType_Camera;
        return true;
    }
    if (reflected_name == "PointLightUBO" || reflected_name == "PhongLight") {
        out_type = UBOType_PointLight;
        return true;
    }
    if (reflected_name == "DirectionalLightUBO") {
        out_type = UBOType_DirectionalLight;
        return true;
    }
    if (reflected_name == "SpotLightUBO") {
        out_type = UBOType_SpotLight;
        return true;
    }
    if (reflected_name == "LightClusterParams") {
        out_type = UBOType_LightClusterParams;
        return true;
    }
    if (reflected_name == "LineGenMeshInfo") {
        out_type = UBOType_LineGenMeshInfo;
        return true;
    }
    return false;
}

bool resolve_ssbo_type(const std::string& reflected_name, SSBOType& out_type) {
    // Match GLSL block/type names from reflection (get_name(base_type_id)).
    if (reflected_name == "PhongInstanceAttrs"
        || reflected_name == "PhongPlusInstanceAttrs"
        || reflected_name == "InstanceAttrs")
    {
        out_type = SSBOType_InstanceAttrs;
        return true;
    }
    if (reflected_name == "PointLights") {
        out_type = SSBOType_PointLights;
        return true;
    }
    if (reflected_name == "ClusterGrid") {
        out_type = SSBOType_ClusterGrid;
        return true;
    }
    if (reflected_name == "ClusterLightIndices") {
        out_type = SSBOType_ClusterLightIndices;
        return true;
    }
    if (reflected_name == "Vertices") {
        out_type = SSBOType_Vertices;
        return true;
    }
    if (reflected_name == "Indices") {
        out_type = SSBOType_Indices;
        return true;
    }
    if (reflected_name == "LineGenParams") {
        out_type = SSBOType_LineGenParams;
        return true;
    }
    return false;
}

const char* compute_ssbo_block_name(SSBOType type) {
    for (const char* block_name : {"PointLights", "ClusterGrid", "ClusterLightIndices"}) {
        SSBOType resolved_type{};
        if (resolve_ssbo_type(block_name, resolved_type) && resolved_type == type) {
            return block_name;
        }
    }
    return nullptr;
}

std::string compute_ssbo_full_name(const std::string& pipeline_name, SSBOType type) {
    const char* block_name = compute_ssbo_block_name(type);
    return block_name == nullptr ? std::string{} : pipeline_name + ":" + block_name;
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

void MeshGPU::sync(const Mesh& mesh, Context* ctx) {
    if (!mesh.loaded)
        throw std::runtime_error("cannot sync unloaded mesh");
    ctx->create_vertex_buffer(mesh.vbuf, vbuf, vbuf_memo, mesh.comp_size, mesh.vcnt);
    ctx->create_index_buffer(reinterpret_cast<const uint32_t*>(mesh.ibuf), ibuf, ibuf_memo, mesh.icnt * 3);
    vcnt = mesh.vcnt;
    icnt = mesh.icnt;
    vert_bytes = static_cast<vk::DeviceSize>(mesh.comp_size) * mesh.vcnt * sizeof(float);
    index_bytes = static_cast<vk::DeviceSize>(mesh.icnt) * 3u * sizeof(uint32_t);
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

void MeshGPU::emit_draw_cmd_instanced(vk::CommandBuffer cmd_buf, vk::PipelineLayout ppl_layout,
    uint32_t instance_count, uint32_t offset, const vk::DescriptorSet* desc_set) const
{
    cmd_buf.bindVertexBuffers(0, *vbuf, {0});
    if (desc_set != nullptr) {
        cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, ppl_layout, 0, {*desc_set}, {});
    }
    cmd_buf.bindIndexBuffer(*ibuf, 0, vk::IndexType::eUint32);
    cmd_buf.drawIndexed(icnt * 3, instance_count, 0, 0, offset);
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

Context::Context(bool enable_debug_m)
    : enable_debug_messenger(enable_debug_m)
{}

uint32_t Context::find_graphics_queue_family_index() const {
    const auto queue_family_properties = physical_device.getQueueFamilyProperties();
    for (uint32_t i = 0; i < queue_family_properties.size(); ++i) {
        if ((queue_family_properties[i].queueFlags & vk::QueueFlagBits::eGraphics)
            && physical_device.getSurfaceSupportKHR(i, *surface))
        {
            return i;
        }
    }
    return ~0u;
}

uint32_t Context::find_compute_queue_family_index(uint32_t preferred_graphics_index) const {
    const auto queue_family_properties = physical_device.getQueueFamilyProperties();
    // Prefer a dedicated compute queue family when available.
    for (uint32_t i = 0; i < queue_family_properties.size(); ++i) {
        const bool supports_compute = (queue_family_properties[i].queueFlags & vk::QueueFlagBits::eCompute) == vk::QueueFlagBits::eCompute;
        const bool supports_graphics = (queue_family_properties[i].queueFlags & vk::QueueFlagBits::eGraphics) == vk::QueueFlagBits::eGraphics;
        if (supports_compute && !supports_graphics) {
            return i;
        }
    }
    for (uint32_t i = 0; i < queue_family_properties.size(); ++i) {
        if ((queue_family_properties[i].queueFlags & vk::QueueFlagBits::eCompute) == vk::QueueFlagBits::eCompute) {
            return i;
        }
    }
    return preferred_graphics_index;
}

vk::DescriptorType Context::to_vk_descriptor_type(ComputeDescriptorKind kind) {
    switch (kind) {
        case ComputeDescriptorKind::UniformBuffer:
            return vk::DescriptorType::eUniformBuffer;
        case ComputeDescriptorKind::StorageBuffer:
            return vk::DescriptorType::eStorageBuffer;
        case ComputeDescriptorKind::CombinedImageSampler:
            return vk::DescriptorType::eCombinedImageSampler;
        case ComputeDescriptorKind::StorageImage:
            return vk::DescriptorType::eStorageImage;
        default:
            throw std::runtime_error("unsupported compute descriptor kind");
    }
}

void Context::setup_debug_messenger() {
    vk::DebugUtilsMessageSeverityFlagsEXT severity_flags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                                          vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    vk::DebugUtilsMessageTypeFlagsEXT message_types(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                                    vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                                                    vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance);
    vk::DebugUtilsMessengerCreateInfoEXT debug_utils_messenger_create_info{};
    debug_utils_messenger_create_info.messageSeverity = severity_flags;
    debug_utils_messenger_create_info.messageType = message_types;
    debug_utils_messenger_create_info.pfnUserCallback = &debug_callback;
    debug_messenger = instance.createDebugUtilsMessengerEXT(debug_utils_messenger_create_info);
}

void Context::transit_presentation_image_layout(
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

vk::Format Context::find_supported_format(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) const {
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

void Context::create_depth_resources() {
    vk::Format depth_format = find_depth_format();
    const bool has_stencil = depth_format == vk::Format::eD32SfloatS8Uint
        || depth_format == vk::Format::eD24UnormS8Uint;
    vk::ImageAspectFlags depth_aspect = vk::ImageAspectFlagBits::eDepth;
    if (has_stencil) {
        depth_aspect |= vk::ImageAspectFlagBits::eStencil;
    }
    std::tie(depth_image, depth_memo) = create_vk_image(swapchain_extent.width, swapchain_extent.height, 1, vk::SampleCountFlagBits::e1, depth_format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal);
    depth_view = create_vk_imageview(depth_image, depth_format, 1, depth_aspect);
    depth_image_initialized = false;
}

void Context::create_swapchain() {
    // create the swapchain
    vk::SurfaceCapabilitiesKHR surface_capabilities = physical_device.getSurfaceCapabilitiesKHR(*surface);
    swapchain_extent = choose_swap_extent(surface_capabilities, window);
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

void Context::create_imageviews() {
    swapchain_image_views.clear();
    swapchain_image_views.reserve(swapchain_images.size());
    for (const auto& image : swapchain_images) {
        swapchain_image_views.emplace_back(create_vk_imageview(image, swapchain_surface_format.format));
    }
}

uint32_t Context::find_memory_type(uint32_t type_filter, vk::MemoryPropertyFlags properties) const {
    vk::PhysicalDeviceMemoryProperties mem_props = physical_device.getMemoryProperties();
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_filter & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type");
}

std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> Context::create_buffer(vk::DeviceSize size,
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

vk::raii::CommandBuffer Context::begin_single_commands(const vk::raii::CommandPool& pool) const {
    vk::CommandBufferAllocateInfo alloc_info{};
    alloc_info.commandPool = *pool;
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    alloc_info.commandBufferCount = 1;
    vk::raii::CommandBuffers command_buffers(device, alloc_info);
    vk::raii::CommandBuffer command_buffer = std::move(command_buffers[0]);

    vk::CommandBufferBeginInfo begin_info{};
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    command_buffer.begin(begin_info);
    return std::move(command_buffer);
}

vk::raii::CommandBuffer Context::begin_single_commands() const {
    return begin_single_commands(command_pool);
}

void Context::end_single_commands(vk::raii::CommandBuffer&& cmd_buf, const vk::raii::Queue& submit_queue) const {
    cmd_buf.end();
    vk::CommandBuffer raw_cmd = *cmd_buf;
    vk::SubmitInfo submit_info{};
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &raw_cmd;
    submit_queue.submit(submit_info, nullptr);
    submit_queue.waitIdle();
}

void Context::end_single_commands(vk::raii::CommandBuffer&& cmd_buf) const {
    end_single_commands(std::move(cmd_buf), queue);
}

void Context::copy_buffer(vk::raii::Buffer& src, vk::raii::Buffer& dst, vk::DeviceSize size) const {
    vk::raii::CommandBuffer cmd_buf = begin_single_commands();
    cmd_buf.copyBuffer(*src, *dst, vk::BufferCopy(0, 0, size));
    end_single_commands(std::move(cmd_buf));
}

std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> Context::load_into_staging_buffer(void* data, uint32_t size) const {
    auto [staging_buf, staging_memo] = create_buffer(size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    void* mapped = staging_memo.mapMemory(0, size);
    std::memcpy(mapped, data, size);
    staging_memo.unmapMemory();
    return std::make_pair(std::move(staging_buf), std::move(staging_memo));
}

void Context::init(GLFWwindow* win,
    const char* app_name,
    uint32_t app_version,
    const char* engine_name,
    uint32_t api_version,
    bool enable_validation_layers,
    const std::vector<const char*>& extra_validation_layers,
    const std::vector<const char*>& extra_extensions)
{
    window = win;

    // init vulkan
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
    if (enable_validation_layers && enable_debug_messenger) {
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

    if (enable_validation_layers && enable_debug_messenger) {
        setup_debug_messenger();
    }

    // create surface
    VkSurfaceKHR raw_surface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(static_cast<VkInstance>(*instance), window, nullptr, &raw_surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface");
    }
    surface = vk::raii::SurfaceKHR(instance, raw_surface);

    std::vector<const char*> required_device_extensions = required_extensions;
    if (use_mesh_shader) {
        required_device_extensions.push_back(vk::EXTMeshShaderExtensionName);
    }

    std::vector<vk::raii::PhysicalDevice> devices = instance.enumeratePhysicalDevices();
    auto const dev_iter = std::ranges::find_if(devices, [&](const auto& device) {
        return is_device_suitable(device, surface, required_device_extensions);
    });
    if (dev_iter == devices.end()) {
        throw std::runtime_error("no suitable Vulkan 1.3 device with dynamic rendering support");
    }
    physical_device = *dev_iter;

    queue_idx = find_graphics_queue_family_index();
    if (queue_idx == ~0u) {
        throw std::runtime_error("no suitable queue family found");
    }
    compute_queue_idx = find_compute_queue_family_index(queue_idx);

    const vk::PhysicalDeviceFeatures supported_features = physical_device.getFeatures();
    sample_rate_shading_enabled = supported_features.sampleRateShading == vk::True;
    const auto supported_feature_chain = physical_device.getFeatures2<
        vk::PhysicalDeviceFeatures2,
        vk::PhysicalDeviceVulkan13Features,
        vk::PhysicalDeviceMeshShaderFeaturesEXT>();
    const bool supports_shader_demote =
        supported_feature_chain.get<vk::PhysicalDeviceVulkan13Features>().shaderDemoteToHelperInvocation == vk::True;
    const bool supports_mesh_shader =
        supported_feature_chain.get<vk::PhysicalDeviceMeshShaderFeaturesEXT>().meshShader == vk::True;
    const bool supports_task_shader =
        supported_feature_chain.get<vk::PhysicalDeviceMeshShaderFeaturesEXT>().taskShader == vk::True;
    if (use_mesh_shader && !supports_mesh_shader) {
        throw std::runtime_error("mesh shader requested but device does not support meshShader feature");
    }

    vk::StructureChain<vk::PhysicalDeviceFeatures2,
        vk::PhysicalDeviceVulkan13Features,
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT,
        vk::PhysicalDeviceMeshShaderFeaturesEXT> device_features;
    device_features.get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy = VK_TRUE;
    device_features.get<vk::PhysicalDeviceFeatures2>().features.sampleRateShading =
        sample_rate_shading_enabled ? VK_TRUE : VK_FALSE;
    device_features.get<vk::PhysicalDeviceVulkan13Features>().synchronization2 = VK_TRUE;
    device_features.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering = VK_TRUE;
    device_features.get<vk::PhysicalDeviceVulkan13Features>().shaderDemoteToHelperInvocation =
        supports_shader_demote ? vk::True : vk::False;
    device_features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState = VK_TRUE;
    device_features.get<vk::PhysicalDeviceMeshShaderFeaturesEXT>().meshShader =
        (use_mesh_shader && supports_mesh_shader) ? vk::True : vk::False;
    device_features.get<vk::PhysicalDeviceMeshShaderFeaturesEXT>().taskShader =
        (use_mesh_shader && supports_task_shader) ? vk::True : vk::False;
    mesh_shader_available = use_mesh_shader && supports_mesh_shader;
    task_shader_available = use_mesh_shader && supports_task_shader;

    float queue_priority = 0.5f;
    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
    queue_create_infos.reserve(queue_idx == compute_queue_idx ? 1 : 2);
    queue_create_infos.push_back(vk::DeviceQueueCreateInfo{}
        .setQueueFamilyIndex(queue_idx)
        .setQueueCount(1)
        .setPQueuePriorities(&queue_priority));
    if (compute_queue_idx != queue_idx) {
        queue_create_infos.push_back(vk::DeviceQueueCreateInfo{}
            .setQueueFamilyIndex(compute_queue_idx)
            .setQueueCount(1)
            .setPQueuePriorities(&queue_priority));
    }
    vk::DeviceCreateInfo device_create_info{};
    device_create_info.pNext = &device_features.get<vk::PhysicalDeviceFeatures2>();
    device_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    device_create_info.pQueueCreateInfos = queue_create_infos.data();
    device_create_info.enabledExtensionCount = static_cast<uint32_t>(required_device_extensions.size());
    device_create_info.ppEnabledExtensionNames = required_device_extensions.data();
    device = vk::raii::Device(physical_device, device_create_info);
    queue = vk::raii::Queue(device, queue_idx, 0);
    compute_queue = vk::raii::Queue(device, compute_queue_idx, 0);

    create_swapchain();
    create_imageviews();

    // create the command pool
    // TODO: eResetCommandBuffer is a mode the command pool kinda persists
    // there's also another flag eTransient that is more like a one-time use pool
    // maybe needed in the future. Might need more methods and fields to 
    // manage the command pool.
    vk::CommandPoolCreateInfo command_pool_create_info{};
    command_pool_create_info.queueFamilyIndex = queue_idx;
    command_pool_create_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    command_pool = vk::raii::CommandPool(device, command_pool_create_info);
    vk::CommandPoolCreateInfo compute_command_pool_create_info{};
    compute_command_pool_create_info.queueFamilyIndex = compute_queue_idx;
    compute_command_pool_create_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    compute_command_pool = vk::raii::CommandPool(device, compute_command_pool_create_info);

    // create the depth resources
    create_depth_resources();

    image_available_semaphores.clear();
    render_finished_semaphores.clear();
    in_flight_fences.clear();
    images_in_flight.assign(swapchain_images.size(), VK_NULL_HANDLE);

    for (size_t i = 0; i < max_frames_in_flight; ++i) {
        image_available_semaphores.emplace_back(device, vk::SemaphoreCreateInfo{});
        vk::FenceCreateInfo fence_create_info{};
        fence_create_info.flags = vk::FenceCreateFlagBits::eSignaled;
        in_flight_fences.emplace_back(device, fence_create_info);
    }
    render_finished_semaphores.reserve(swapchain_images.size());
    for (size_t i = 0; i < swapchain_images.size(); ++i) {
        render_finished_semaphores.emplace_back(device, vk::SemaphoreCreateInfo{});
    }

    vk::CommandBufferAllocateInfo command_buffer_alloc_info{};
    command_buffer_alloc_info.commandPool = *command_pool;
    command_buffer_alloc_info.level = vk::CommandBufferLevel::ePrimary;
    command_buffer_alloc_info.commandBufferCount = static_cast<uint32_t>(swapchain_images.size());
    command_buffers = vk::raii::CommandBuffers(device, command_buffer_alloc_info);
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

bool Context::create_pipeline(const std::string& name,
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
    std::map<uint32_t, vk::DescriptorSetLayoutBinding> descriptor_bindings;
    std::map<uint32_t, UBOType> ubo_binding_to_type;
    std::map<uint32_t, SSBOType> storage_binding_to_type;
    std::map<uint32_t, std::string> tex_binding_to_name;
    std::unordered_map<UBOType, UBO> pipeline_ubos;
    std::unordered_map<SSBOType, SSBO> pipeline_ssbos;
    const bool pipeline_uses_mesh_shader = shader_module_pack.uses_mesh_shader()
        || shader_module_pack.modules.contains(vk::ShaderStageFlagBits::eMeshEXT)
        || shader_module_pack.modules.contains(vk::ShaderStageFlagBits::eTaskEXT);
    bool has_vertex_stage = false;
    bool has_mesh_stage = false;
    bool has_task_stage = false;

    if (pipeline_uses_mesh_shader && (!use_mesh_shader || !mesh_shader_available)) {
        std::cout << "Pipeline " << name << " requested mesh shader, but mesh shader support is disabled." << std::endl;
        return false;
    }

    std::vector<vk::raii::ShaderModule> shader_modules;
    std::vector<vk::PipelineShaderStageCreateInfo> shader_stage_infos;
    shader_modules.reserve(shader_module_pack.modules.size());
    shader_stage_infos.reserve(shader_module_pack.modules.size());

    const auto merge_descriptor_binding = [&descriptor_bindings](const vk::DescriptorSetLayoutBinding& binding) {
        const auto found = descriptor_bindings.find(binding.binding);
        if (found == descriptor_bindings.end()) {
            descriptor_bindings.emplace(binding.binding, binding);
            return;
        }
        found->second.stageFlags |= binding.stageFlags;
    };

    for (const auto& [stage, module] : shader_module_pack.modules) {
        vk::ShaderModuleCreateInfo shader_module_create_info{};
        shader_module_create_info.codeSize = module.spirv_code.size() * sizeof(uint32_t);
        shader_module_create_info.pCode = module.spirv_code.data();
        shader_modules.emplace_back(device, shader_module_create_info);

        vk::PipelineShaderStageCreateInfo shader_stage_info{};
        shader_stage_info.stage = stage;
        shader_stage_info.module = *shader_modules.back();
        shader_stage_info.pName = "main";
        shader_stage_infos.push_back(shader_stage_info);

        if (stage == vk::ShaderStageFlagBits::eTaskEXT) {
            has_task_stage = true;
        }
        if (stage == vk::ShaderStageFlagBits::eMeshEXT) {
            has_mesh_stage = true;
        }
        if (stage == vk::ShaderStageFlagBits::eVertex) {
            has_vertex_stage = true;
            input_binding_descs = gen_binding_desc(comps, interleaved);

            uint32_t offset = 0;
            int binding_idx = 0;
            for (const auto& [attr_loc, glsl_type, attr_name] : module.attr_infos) {
                (void)attr_name;
                if (interleaved) {
                    vk::VertexInputAttributeDescription attr_desc{};
                    attr_desc.location = attr_loc;
                    attr_desc.binding = 0;
                    attr_desc.format = static_cast<vk::Format>(glsl_type_macro[glsl_type]);
                    attr_desc.offset = offset;
                    input_attr_descs.push_back(attr_desc);
                    offset += glsl_type_sizes[glsl_type];
                }
                else {
                    vk::VertexInputAttributeDescription attr_desc{};
                    attr_desc.location = attr_loc;
                    attr_desc.binding = static_cast<uint32_t>(binding_idx);
                    attr_desc.format = static_cast<vk::Format>(glsl_type_macro[glsl_type]);
                    attr_desc.offset = 0;
                    input_attr_descs.push_back(attr_desc);
                }
                ++binding_idx;
            }
        }

        for (const auto& [ubo_name, ubo_info] : module.buf_infos) {
            const auto& [struct_size, array_size, binding] = ubo_info;
            UBOType ubo_type{};
            if (!resolve_ubo_type(ubo_name, ubo_type)) {
                assert(false && "Unsupported UBO type in create_pipeline reflection.");
                continue;
            }
            const uint32_t alloc_size = struct_size == 0 ? 16u : struct_size;
            add_ubo(pipeline_ubos, ubo_type, binding, alloc_size, array_size);
            ubo_binding_to_type[binding] = ubo_type;

            vk::DescriptorSetLayoutBinding layout_binding{};
            layout_binding.binding = binding;
            layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
            layout_binding.descriptorCount = array_size;
            layout_binding.stageFlags = stage;
            merge_descriptor_binding(layout_binding);
        }

        if (!module.storage_buf_infos.empty()) {
            for (const auto& [ssbo_name, ssbo_info] : module.storage_buf_infos) {
                SSBOType ssbo_type{};
                if (!resolve_ssbo_type(ssbo_name, ssbo_type)) {
                    std::cout << "Unsupported SSBO type " << ssbo_name
                        << " in pipeline " << name << std::endl;
                    return false;
                }

                const auto& [struct_size, array_size, binding] = ssbo_info;
                const auto existing = pipeline_ssbos.find(ssbo_type);
                if (existing == pipeline_ssbos.end()) {
                    SSBO ssbo{};
                    ssbo.size = struct_size == 0 ? 16u : struct_size;
                    ssbo.vecsize = array_size;
                    ssbo.binding = binding;
                    ssbo.descriptor_type = vk::DescriptorType::eStorageBuffer;
                    pipeline_ssbos.emplace(ssbo_type, std::move(ssbo));
                }
                else if (existing->second.binding != binding) {
                    std::cout << "SSBO " << ssbo_name << " uses inconsistent bindings in pipeline "
                        << name << std::endl;
                    return false;
                }

                storage_binding_to_type[binding] = ssbo_type;
                vk::DescriptorSetLayoutBinding layout_binding{};
                layout_binding.binding = binding;
                layout_binding.descriptorType = vk::DescriptorType::eStorageBuffer;
                layout_binding.descriptorCount = 1;
                layout_binding.stageFlags = stage;
                merge_descriptor_binding(layout_binding);
            }
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

            vk::DescriptorSetLayoutBinding tex_layout_binding{};
            tex_layout_binding.binding = tex_binding;
            tex_layout_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
            tex_layout_binding.descriptorCount = descriptor_count;
            tex_layout_binding.stageFlags = stage;
            merge_descriptor_binding(tex_layout_binding);
        }
    }

    std::vector<vk::DescriptorSetLayoutBinding> descriptor_layouts;
    descriptor_layouts.reserve(descriptor_bindings.size());
    for (const auto& [_, binding] : descriptor_bindings) {
        descriptor_layouts.push_back(binding);
    }

    if (!pipeline_uses_mesh_shader && !has_vertex_stage) {
        std::cout << "Pipeline " << name << " has no vertex stage." << std::endl;
        return false;
    }
    if (pipeline_uses_mesh_shader && has_vertex_stage) {
        std::cout << "Pipeline " << name << " mixes mesh/task and vertex shader stages." << std::endl;
        return false;
    }
    if (has_task_stage && !task_shader_available) {
        std::cout << "Pipeline " << name << " uses task shader, but task shader support is disabled." << std::endl;
        return false;
    }
    if (has_task_stage && !has_mesh_stage) {
        std::cout << "Pipeline " << name << " has a task stage without a mesh stage." << std::endl;
        return false;
    }
    if (pipeline_uses_mesh_shader && !has_mesh_stage) {
        std::cout << "Pipeline " << name << " requested mesh shader path without a mesh stage." << std::endl;
        return false;
    }

    PipelineOption local_option = option;
    local_option.vert_info.vertexBindingDescriptionCount = static_cast<uint32_t>(input_binding_descs.size());
    local_option.vert_info.pVertexBindingDescriptions = input_binding_descs.data();
    local_option.vert_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(input_attr_descs.size());
    local_option.vert_info.pVertexAttributeDescriptions = input_attr_descs.data();

    vk::raii::DescriptorSetLayout descriptor_set_layout{nullptr};
    if (!descriptor_layouts.empty()) {
        vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info{};
        descriptor_set_layout_info.bindingCount = static_cast<uint32_t>(descriptor_layouts.size());
        descriptor_set_layout_info.pBindings = descriptor_layouts.data();
        descriptor_set_layout = vk::raii::DescriptorSetLayout(device, descriptor_set_layout_info);
    }

    vk::DescriptorSetLayout set_layout_handle = descriptor_set_layout != nullptr ? static_cast<vk::DescriptorSetLayout>(*descriptor_set_layout) : nullptr;
    vk::PipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.setLayoutCount = descriptor_set_layout != nullptr ? 1u : 0u;
    pipeline_layout_info.pSetLayouts = descriptor_set_layout != nullptr ? &set_layout_handle : nullptr;
    vk::raii::PipelineLayout pipeline_layout(device, pipeline_layout_info);

    const vk::Format depth_format = find_depth_format();
    vk::PipelineRenderingCreateInfo rendering_create_info{};
    rendering_create_info.colorAttachmentCount = 1;
    rendering_create_info.pColorAttachmentFormats = &swapchain_surface_format.format;
    rendering_create_info.depthAttachmentFormat = depth_format;
    rendering_create_info.stencilAttachmentFormat = depth_format;
    vk::GraphicsPipelineCreateInfo graphics_pipeline_create_info{};
    graphics_pipeline_create_info.pNext = &rendering_create_info;
    graphics_pipeline_create_info.stageCount = static_cast<uint32_t>(shader_stage_infos.size());
    graphics_pipeline_create_info.pStages = shader_stage_infos.data();
    graphics_pipeline_create_info.pVertexInputState = pipeline_uses_mesh_shader ? nullptr : &local_option.vert_info;
    graphics_pipeline_create_info.pInputAssemblyState = pipeline_uses_mesh_shader ? nullptr : &local_option.assembly_info;
    graphics_pipeline_create_info.pViewportState = &local_option.viewport_info;
    graphics_pipeline_create_info.pRasterizationState = &local_option.raster_info;
    graphics_pipeline_create_info.pMultisampleState = &local_option.multisample_info;
    graphics_pipeline_create_info.pDepthStencilState = &local_option.depth_info;
    graphics_pipeline_create_info.pColorBlendState = &local_option.blend_info;
    graphics_pipeline_create_info.pDynamicState = &local_option.dynamic_info;
    graphics_pipeline_create_info.layout = *pipeline_layout;
    graphics_pipeline_create_info.renderPass = nullptr;
    graphics_pipeline_create_info.subpass = 0;
    vk::raii::Pipelines vk_pipelines(device, nullptr, {graphics_pipeline_create_info});
    if (vk_pipelines.empty()) {
        std::cout << "Pipeline " << name << " creation failed" << std::endl;
        return false;
    }
    vk::raii::Pipeline vk_pipeline = std::move(vk_pipelines.front());

    Pipeline pipeline;
    pipeline.vk_pipeline = std::move(vk_pipeline);
    pipeline.vk_pipeline_layout = std::move(pipeline_layout);
    pipeline.descriptor_set_layout = std::move(descriptor_set_layout);
    pipeline.uses_mesh_shader = pipeline_uses_mesh_shader;
    pipeline.ubos = std::move(pipeline_ubos);
    pipeline.ssbos = std::move(pipeline_ssbos);

    const uint32_t swapchain_cnt = static_cast<uint32_t>(swapchain_images.size());
    uint32_t uniform_desc_count = 0;
    for (const auto& [binding, ubo_type] : ubo_binding_to_type) {
        (void)binding;
        const auto ubo_found = pipeline.ubos.find(ubo_type);
        assert(ubo_found != pipeline.ubos.end());
        uniform_desc_count += static_cast<uint32_t>(ubo_found->second.vecsize);
    }
    uint32_t storage_desc_count = 0;
    for (const auto& [binding, ssbo_type] : storage_binding_to_type) {
        (void)binding;
        const auto ssbo_found = pipeline.ssbos.find(ssbo_type);
        assert(ssbo_found != pipeline.ssbos.end());
        storage_desc_count += 1;
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
        vk::DescriptorPoolSize pool_size{};
        pool_size.type = vk::DescriptorType::eUniformBuffer;
        pool_size.descriptorCount = uniform_desc_count * swapchain_cnt;
        pool_sizes.push_back(pool_size);
    }
    if (image_desc_count > 0) {
        vk::DescriptorPoolSize pool_size{};
        pool_size.type = vk::DescriptorType::eCombinedImageSampler;
        pool_size.descriptorCount = image_desc_count * swapchain_cnt;
        pool_sizes.push_back(pool_size);
    }
    if (storage_desc_count > 0) {
        vk::DescriptorPoolSize pool_size{};
        pool_size.type = vk::DescriptorType::eStorageBuffer;
        pool_size.descriptorCount = storage_desc_count * swapchain_cnt;
        pool_sizes.push_back(pool_size);
    }

    if (!pool_sizes.empty()) {
        vk::DescriptorPoolCreateInfo descriptor_pool_info{};
        descriptor_pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
        descriptor_pool_info.maxSets = swapchain_cnt;
        descriptor_pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
        descriptor_pool_info.pPoolSizes = pool_sizes.data();
        pipeline.descriptor_pool = vk::raii::DescriptorPool(device, descriptor_pool_info);

        std::vector<vk::DescriptorSetLayout> set_layouts(swapchain_cnt, *pipeline.descriptor_set_layout);
        vk::DescriptorSetAllocateInfo descriptor_set_alloc_info{};
        descriptor_set_alloc_info.descriptorPool = *pipeline.descriptor_pool;
        descriptor_set_alloc_info.descriptorSetCount = swapchain_cnt;
        descriptor_set_alloc_info.pSetLayouts = set_layouts.data();
        pipeline.descriptor_sets = vk::raii::DescriptorSets(device, descriptor_set_alloc_info);

        for (uint32_t i = 0; i < swapchain_cnt; ++i) {
            std::vector<vk::DescriptorBufferInfo> buffer_infos;
            std::vector<vk::DescriptorImageInfo> image_infos;
            std::vector<vk::WriteDescriptorSet> writes;
            buffer_infos.reserve(ubo_binding_to_type.size() + storage_binding_to_type.size());
            image_infos.reserve(tex_binding_to_name.size());
            writes.reserve(ubo_binding_to_type.size() + storage_binding_to_type.size() + tex_binding_to_name.size());

            for (const auto& [binding, ubo_type] : ubo_binding_to_type) {
                const auto ubo_found = pipeline.ubos.find(ubo_type);
                assert(ubo_found != pipeline.ubos.end());
                auto& ubo = ubo_found->second;
                vk::DescriptorBufferInfo buffer_info{};
                buffer_info.buffer = *ubo.gpu_bufs[i];
                buffer_info.offset = 0;
                buffer_info.range = ubo.size * ubo.vecsize;
                buffer_infos.push_back(buffer_info);
                vk::WriteDescriptorSet write{};
                write.dstSet = *pipeline.descriptor_sets[i];
                write.dstBinding = binding;
                write.descriptorCount = 1;
                write.descriptorType = vk::DescriptorType::eUniformBuffer;
                write.pBufferInfo = &buffer_infos.back();
                writes.push_back(write);
            }

            for (const auto& [binding, ssbo_type] : storage_binding_to_type) {
                const auto ssbo_found = pipeline.ssbos.find(ssbo_type);
                assert(ssbo_found != pipeline.ssbos.end());
                const auto& ssbo = ssbo_found->second;
                if (ssbo.uses_borrowed_descriptors) {
                    if (i >= ssbo.borrowed_descriptors.size()) {
                        continue;
                    }
                    buffer_infos.push_back(ssbo.borrowed_descriptors[i]);
                }
                else if (i < ssbo.gpu_bufs.size()) {
                    vk::DescriptorBufferInfo buffer_info{};
                    buffer_info.buffer = *ssbo.gpu_bufs[i];
                    buffer_info.offset = 0;
                    buffer_info.range = ssbo.size * ssbo.vecsize;
                    buffer_infos.push_back(buffer_info);
                }
                else {
                    continue;
                }
                vk::WriteDescriptorSet write{};
                write.dstSet = *pipeline.descriptor_sets[i];
                write.dstBinding = binding;
                write.descriptorCount = 1;
                write.descriptorType = vk::DescriptorType::eStorageBuffer;
                write.pBufferInfo = &buffer_infos.back();
                writes.push_back(write);
            }

            for (const auto& [binding, tex_name] : tex_binding_to_name) {
                const auto tex_found = textures.find(tex_name);
                if (tex_found == textures.end()) {
                    continue;
                }
                image_infos.push_back(tex_found->second.descriptor);
                vk::WriteDescriptorSet write{};
                write.dstSet = *pipeline.descriptor_sets[i];
                write.dstBinding = binding;
                write.descriptorCount = 1;
                write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
                write.pImageInfo = &image_infos.back();
                writes.push_back(write);
            }

            if (!writes.empty()) {
                device.updateDescriptorSets(writes, {});
            }
        }
    }

    pipelines.emplace(name, std::move(pipeline));
    return true;
}

bool Context::load_compute_pipeline(const std::string& name, const fs::path& path) {
    ComputeShader shader;
    if (!shader.load(path)) {
        return false;
    }
    return create_compute_pipeline(name, shader);
}

bool Context::create_compute_pipeline(const std::string& name, const ComputeShader& shader) {
    if (compute_pipelines.find(name) != compute_pipelines.end()) {
        std::cout << "Compute pipeline " << name << " already exists" << std::endl;
        return false;
    }
    if (shader.spirv_code.empty()) {
        std::cout << "Compute shader for " << name << " is empty" << std::endl;
        return false;
    }

    std::vector<vk::DescriptorSetLayoutBinding> descriptor_layouts;
    descriptor_layouts.reserve(shader.bindings.size());
    std::map<uint32_t, std::string> ubo_binding_to_name;
    std::map<uint32_t, std::string> ssbo_binding_to_name;
    std::map<uint32_t, std::string> tex_binding_to_name;
    std::unordered_map<std::string, UBO> compute_ubos;
    std::unordered_map<std::string, SSBO> compute_ssbos;

    for (const auto& binding : shader.bindings) {
        if (binding.kind == ComputeDescriptorKind::StorageImage)
        {
            std::cout << "Compute descriptor kind for binding " << binding.name
                << " is not yet wired in Context resource manager" << std::endl;
            return false;
        }

        if (binding.kind == ComputeDescriptorKind::UniformBuffer) {
            const auto ubo_name = name + ":" + binding.name;
            const uint32_t struct_size = binding.struct_size == 0 ? 16u : binding.struct_size;
            add_ubo(compute_ubos, ubo_name, binding.binding, struct_size, binding.descriptor_count);
            ubo_binding_to_name[binding.binding] = ubo_name;
        }
        else if (binding.kind == ComputeDescriptorKind::StorageBuffer) {
            const auto ssbo_name = name + ":" + binding.name;
            SSBO ssbo{};
            ssbo.size = binding.struct_size == 0 ? 16u : binding.struct_size;
            ssbo.vecsize = 0;
            ssbo.binding = binding.binding;
            ssbo.descriptor_type = vk::DescriptorType::eStorageBuffer;
            compute_ssbos.emplace(ssbo_name, std::move(ssbo));
            ssbo_binding_to_name[binding.binding] = ssbo_name;
        }
        else if (binding.kind == ComputeDescriptorKind::CombinedImageSampler) {
            const auto tex_name = name + ":" + binding.name;
            tex_binding_to_name[binding.binding] = tex_name;
        }

        vk::DescriptorSetLayoutBinding layout_binding{};
        layout_binding.binding = binding.binding;
        layout_binding.descriptorType = to_vk_descriptor_type(binding.kind);
        layout_binding.descriptorCount = binding.descriptor_count;
        layout_binding.stageFlags = vk::ShaderStageFlagBits::eCompute;
        descriptor_layouts.push_back(layout_binding);
    }

    vk::raii::ShaderModule shader_module{nullptr};
    {
        vk::ShaderModuleCreateInfo shader_module_create_info{};
        shader_module_create_info.codeSize = shader.spirv_code.size() * sizeof(uint32_t);
        shader_module_create_info.pCode = shader.spirv_code.data();
        shader_module = vk::raii::ShaderModule(device, shader_module_create_info);
    }

    vk::raii::DescriptorSetLayout descriptor_set_layout{nullptr};
    if (!descriptor_layouts.empty()) {
        vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info{};
        descriptor_set_layout_info.bindingCount = static_cast<uint32_t>(descriptor_layouts.size());
        descriptor_set_layout_info.pBindings = descriptor_layouts.data();
        descriptor_set_layout = vk::raii::DescriptorSetLayout(device, descriptor_set_layout_info);
    }

    vk::DescriptorSetLayout set_layout_handle = descriptor_set_layout != nullptr
        ? static_cast<vk::DescriptorSetLayout>(*descriptor_set_layout)
        : nullptr;
    vk::PipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.setLayoutCount = descriptor_set_layout != nullptr ? 1u : 0u;
    pipeline_layout_info.pSetLayouts = descriptor_set_layout != nullptr ? &set_layout_handle : nullptr;
    vk::raii::PipelineLayout pipeline_layout(device, pipeline_layout_info);

    vk::PipelineShaderStageCreateInfo shader_stage_info{};
    shader_stage_info.stage = vk::ShaderStageFlagBits::eCompute;
    shader_stage_info.module = *shader_module;
    shader_stage_info.pName = "main";

    vk::ComputePipelineCreateInfo compute_pipeline_create_info{};
    compute_pipeline_create_info.stage = shader_stage_info;
    compute_pipeline_create_info.layout = *pipeline_layout;
    vk::raii::Pipeline compute_pipeline(device, nullptr, compute_pipeline_create_info);

    ComputePipeline pipeline{};
    pipeline.vk_pipeline = std::move(compute_pipeline);
    pipeline.vk_pipeline_layout = std::move(pipeline_layout);
    pipeline.descriptor_set_layout = std::move(descriptor_set_layout);
    pipeline.ubos = std::move(compute_ubos);
    pipeline.ssbos = std::move(compute_ssbos);

    const uint32_t swapchain_cnt = static_cast<uint32_t>(swapchain_images.size());
    std::unordered_map<vk::DescriptorType, uint32_t> descriptor_type_counts;
    for (const auto& binding : shader.bindings) {
        const auto descriptor_type = to_vk_descriptor_type(binding.kind);
        descriptor_type_counts[descriptor_type] += binding.descriptor_count * swapchain_cnt;
    }
    std::vector<vk::DescriptorPoolSize> pool_sizes;
    pool_sizes.reserve(descriptor_type_counts.size());
    for (const auto& [descriptor_type, descriptor_count] : descriptor_type_counts) {
        vk::DescriptorPoolSize pool_size{};
        pool_size.type = descriptor_type;
        pool_size.descriptorCount = descriptor_count;
        pool_sizes.push_back(pool_size);
    }

    if (!pool_sizes.empty()) {
        vk::DescriptorPoolCreateInfo descriptor_pool_info{};
        descriptor_pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
        descriptor_pool_info.maxSets = swapchain_cnt;
        descriptor_pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
        descriptor_pool_info.pPoolSizes = pool_sizes.data();
        pipeline.descriptor_pool = vk::raii::DescriptorPool(device, descriptor_pool_info);

        std::vector<vk::DescriptorSetLayout> set_layouts(swapchain_cnt, *pipeline.descriptor_set_layout);
        vk::DescriptorSetAllocateInfo descriptor_set_alloc_info{};
        descriptor_set_alloc_info.descriptorPool = *pipeline.descriptor_pool;
        descriptor_set_alloc_info.descriptorSetCount = swapchain_cnt;
        descriptor_set_alloc_info.pSetLayouts = set_layouts.data();
        pipeline.descriptor_sets = vk::raii::DescriptorSets(device, descriptor_set_alloc_info);

        for (uint32_t i = 0; i < swapchain_cnt; ++i) {
            std::vector<vk::DescriptorBufferInfo> buffer_infos;
            std::vector<vk::DescriptorImageInfo> image_infos;
            std::vector<vk::WriteDescriptorSet> writes;
            buffer_infos.reserve(ubo_binding_to_name.size() + ssbo_binding_to_name.size());
            image_infos.reserve(tex_binding_to_name.size());
            writes.reserve(ubo_binding_to_name.size() + ssbo_binding_to_name.size() + tex_binding_to_name.size());

            for (const auto& [binding, ubo_name] : ubo_binding_to_name) {
                const auto ubo_found = pipeline.ubos.find(ubo_name);
                if (ubo_found == pipeline.ubos.end()) {
                    continue;
                }
                auto& ubo = ubo_found->second;
                vk::DescriptorBufferInfo buffer_info{};
                buffer_info.buffer = *ubo.gpu_bufs[i];
                buffer_info.offset = 0;
                buffer_info.range = ubo.size * ubo.vecsize;
                buffer_infos.push_back(buffer_info);

                vk::WriteDescriptorSet write{};
                write.dstSet = *pipeline.descriptor_sets[i];
                write.dstBinding = binding;
                write.descriptorCount = 1;
                write.descriptorType = vk::DescriptorType::eUniformBuffer;
                write.pBufferInfo = &buffer_infos.back();
                writes.push_back(write);
            }

            // Compute SSBOs are allocated explicitly after the caller sets capacity.
            for (const auto& [binding, tex_name] : tex_binding_to_name) {
                const auto tex_found = textures.find(tex_name);
                if (tex_found == textures.end()) {
                    std::cout << "Compute texture " << tex_name << " not found for pipeline " << name << std::endl;
                    return false;
                }
                image_infos.push_back(tex_found->second.descriptor);

                vk::WriteDescriptorSet write{};
                write.dstSet = *pipeline.descriptor_sets[i];
                write.dstBinding = binding;
                write.descriptorCount = 1;
                write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
                write.pImageInfo = &image_infos.back();
                writes.push_back(write);
            }

            if (!writes.empty()) {
                device.updateDescriptorSets(writes, {});
            }
        }
    }

    compute_pipelines.emplace(name, std::move(pipeline));
    return true;
}

bool Context::draw_mesh_tasks(vk::CommandBuffer cmd, uint32_t group_x, uint32_t group_y,
    uint32_t group_z) const
{
    const auto draw_mesh_tasks_fn = reinterpret_cast<PFN_vkCmdDrawMeshTasksEXT>(
        device.getProcAddr("vkCmdDrawMeshTasksEXT"));
    if (draw_mesh_tasks_fn == nullptr) {
        std::cout << "vkCmdDrawMeshTasksEXT is not available on this device" << std::endl;
        return false;
    }
    draw_mesh_tasks_fn(static_cast<VkCommandBuffer>(cmd), group_x, group_y, group_z);
    return true;
}

bool Context::record_mesh_tasks(vk::CommandBuffer cmd, const std::string& pipeline_name,
    uint32_t group_x, uint32_t group_y, uint32_t group_z, uint32_t descriptor_set_index) const
{
    const auto found = pipelines.find(pipeline_name);
    if (found == pipelines.end()) {
        return false;
    }

    const auto& pipeline = found->second;
    if (!pipeline.uses_mesh_shader) {
        return false;
    }

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline.vk_pipeline);
    if (!pipeline.descriptor_sets.empty()) {
        const auto idx = std::min<uint32_t>(descriptor_set_index,
            static_cast<uint32_t>(pipeline.descriptor_sets.size() - 1));
        const vk::DescriptorSet desc_set = *pipeline.descriptor_sets[idx];
        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, *pipeline.vk_pipeline_layout, 0, {desc_set}, {});
    }
    return draw_mesh_tasks(cmd, group_x, group_y, group_z);
}

bool Context::draw_mesh_instanced(vk::CommandBuffer cmd, const std::string& mesh_name,
    vk::PipelineLayout pipeline_layout, uint32_t instance_count, uint32_t ssbo_offset,
    const vk::DescriptorSet* desc_set) const
{
    const auto mesh_found = meshes.find(mesh_name);
    if (mesh_found == meshes.end()) {
        return false;
    }
    mesh_found->second.emit_draw_cmd_instanced(cmd, pipeline_layout, instance_count, ssbo_offset, desc_set);
    return true;
}

void Context::dispatch_compute(const std::string& pipeline_name, uint32_t group_x, uint32_t group_y,
    uint32_t group_z, uint32_t descriptor_set_index)
{
    const auto found = compute_pipelines.find(pipeline_name);
    if (found == compute_pipelines.end()) {
        std::cout << "Compute pipeline " << pipeline_name << " not found" << std::endl;
        return;
    }

    auto cmd = begin_single_commands(compute_command_pool);
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *found->second.vk_pipeline);
    if (!found->second.descriptor_sets.empty()) {
        const auto idx = std::min<uint32_t>(descriptor_set_index,
            static_cast<uint32_t>(found->second.descriptor_sets.size() - 1));
        vk::DescriptorSet desc_set = *found->second.descriptor_sets[idx];
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *found->second.vk_pipeline_layout, 0, {desc_set}, {});
    }
    cmd.dispatch(group_x, group_y, group_z);
    end_single_commands(std::move(cmd), compute_queue);
}

bool Context::record_compute(vk::CommandBuffer cmd, const std::string& pipeline_name,
    uint32_t group_x, uint32_t group_y, uint32_t group_z, uint32_t descriptor_set_index) const
{
    const auto found = compute_pipelines.find(pipeline_name);
    if (found == compute_pipelines.end()) {
        return false;
    }

    const auto& pipeline = found->second;
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline.vk_pipeline);
    if (!pipeline.descriptor_sets.empty()) {
        const auto idx = std::min<uint32_t>(descriptor_set_index,
            static_cast<uint32_t>(pipeline.descriptor_sets.size() - 1));
        const vk::DescriptorSet desc_set = *pipeline.descriptor_sets[idx];
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipeline.vk_pipeline_layout, 0, {desc_set}, {});
    }
    cmd.dispatch(group_x, group_y, group_z);

    vk::MemoryBarrier2 barrier{};
    barrier.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader;
    barrier.srcAccessMask = vk::AccessFlagBits2::eShaderStorageWrite;
    barrier.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands;
    barrier.dstAccessMask = vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite;
    vk::DependencyInfo dependency_info{};
    dependency_info.memoryBarrierCount = 1;
    dependency_info.pMemoryBarriers = &barrier;
    cmd.pipelineBarrier2(dependency_info);
    return true;
}

void Context::record_cmds(uint32_t image_index,
    const std::function<void(vk::raii::CommandBuffer&, uint32_t)>& emit_func,
    const std::function<void(vk::raii::CommandBuffer&, uint32_t)>& pre_render_func)
{
    auto& cmd_buf = command_buffers[image_index];
    cmd_buf.reset({});
    cmd_buf.begin({});

    Texture* active_target = nullptr;
    if (active_render_target_index_ >= 0
        && static_cast<size_t>(active_render_target_index_) < targets.size())
    {
        active_target = &targets[static_cast<size_t>(active_render_target_index_)];
    }

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

    if (!depth_image_initialized) {
        transit_presentation_image_layout(
            cmd_buf,
            static_cast<vk::Image>(*depth_image),
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eDepthStencilAttachmentOptimal,
            {},
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::PipelineStageFlagBits2::eTopOfPipe,
            vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
            vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);
        depth_image_initialized = true;
    }
    else {
        // Keep depth image in attachment layout across frames and insert
        // an explicit dependency between consecutive depth write passes.
        transit_presentation_image_layout(
            cmd_buf,
            static_cast<vk::Image>(*depth_image),
            vk::ImageLayout::eDepthStencilAttachmentOptimal,
            vk::ImageLayout::eDepthStencilAttachmentOptimal,
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::PipelineStageFlagBits2::eLateFragmentTests,
            vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
            vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);
    }

    vk::ClearValue clear_value = vk::ClearColorValue(std::array<float, 4>{0.f, 0.f, 0.f, 1.f});
    vk::ClearValue depth_clear_value = vk::ClearDepthStencilValue(1.f, 0);

    if (active_target != nullptr) {
        transit_presentation_image_layout(
            cmd_buf,
            static_cast<vk::Image>(*active_target->image),
            active_target->layout,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
            vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::PipelineStageFlagBits2::eAllCommands,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::ImageAspectFlagBits::eColor);
    }

    vk::RenderingAttachmentInfo color_attachment_info{};
    color_attachment_info.imageView = active_target == nullptr
        ? *swapchain_image_views[image_index]
        : *active_target->view;
    color_attachment_info.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    color_attachment_info.loadOp = vk::AttachmentLoadOp::eClear;
    color_attachment_info.storeOp = vk::AttachmentStoreOp::eStore;
    color_attachment_info.clearValue = clear_value;
    vk::RenderingAttachmentInfo depth_attachment_info{};
    depth_attachment_info.imageView = *depth_view;
    depth_attachment_info.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    depth_attachment_info.loadOp = vk::AttachmentLoadOp::eClear;
    depth_attachment_info.storeOp = vk::AttachmentStoreOp::eStore;
    depth_attachment_info.clearValue = depth_clear_value;
    vk::RenderingInfo rendering_info{};
    rendering_info.renderArea.offset = vk::Offset2D{0, 0};
    const vk::Extent2D render_extent = active_target == nullptr
        ? swapchain_extent
        : vk::Extent2D{active_target->width, active_target->height};
    rendering_info.renderArea.extent = render_extent;
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &color_attachment_info;
    rendering_info.pDepthAttachment = &depth_attachment_info;
    if (pre_render_func) {
        pre_render_func(cmd_buf, image_index);
    }
    cmd_buf.beginRendering(rendering_info);
    cmd_buf.setViewport(0, vk::Viewport{
        0.f, 0.f,
        static_cast<float>(render_extent.width),
        static_cast<float>(render_extent.height),
        0.f, 1.f
    });
    cmd_buf.setScissor(0, vk::Rect2D{{0, 0}, render_extent});

    emit_func(cmd_buf, image_index);

    cmd_buf.endRendering();

    if (active_target != nullptr) {
        transit_presentation_image_layout(
            cmd_buf,
            static_cast<vk::Image>(*active_target->image),
            vk::ImageLayout::eColorAttachmentOptimal,
            active_target->layout,
            vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::PipelineStageFlagBits2::eAllCommands,
            vk::ImageAspectFlagBits::eColor);
    }
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

void Context::draw_frame() {
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
    vk::SubmitInfo submit_info{};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &*image_available_semaphores[current_frame];
    submit_info.pWaitDstStageMask = &wait_stage_mask;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &*command_buffers[image_index];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &*render_finished_semaphores[image_index];
    queue.submit(submit_info, *in_flight_fences[current_frame]);

    vk::SwapchainKHR swapchains[] = {*swapchain};
    vk::PresentInfoKHR present_info{};
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &*render_finished_semaphores[image_index];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchains;
    present_info.pImageIndices = &image_index;
    result = queue.presentKHR(present_info);
    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || frame_buffer_resized) {
        frame_buffer_resized = false;
        recreate_swapchain();
    }

    current_frame = (current_frame + 1) % max_frames_in_flight;
}

bool Context::add_ubo(std::unordered_map<UBOType, UBO>& ubos, UBOType type, uint32_t binding,
    uint32_t size, uint32_t vecsize,
    vk::BufferUsageFlags usage,
    vk::DescriptorType descriptor_type)
{
    const auto found = ubos.find(type);
    if (found != ubos.end()) {
        return true;
    }

    UBO ubo{};
    ubo.size = size;
    ubo.vecsize = vecsize;
    ubo.binding = binding;
    ubo.descriptor_type = descriptor_type;
    //ubo.cpu_buf = std::make_shared<char[]>(size * vecsize);
    ubo.gpu_bufs.reserve(swapchain_images.size());
    ubo.memos.reserve(swapchain_images.size());
    ubo.descriptors.reserve(swapchain_images.size());

    for (size_t i = 0; i < swapchain_images.size(); ++i) {
        vk::raii::Buffer gpu_buf{nullptr};
        vk::raii::DeviceMemory memo{nullptr};
        std::tie(gpu_buf, memo) = create_buffer(
            size * vecsize,
            usage,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        ubo.descriptors.push_back(vk::DescriptorBufferInfo{*gpu_buf, 0, size * vecsize});
        ubo.gpu_bufs.push_back(std::move(gpu_buf));
        ubo.memos.push_back(std::move(memo));
    }

    ubos.emplace(type, std::move(ubo));
    return true;
}

bool Context::add_ubo(std::unordered_map<std::string, UBO>& ubos, const std::string& name, uint32_t binding,
    uint32_t size, uint32_t vecsize,
    vk::BufferUsageFlags usage,
    vk::DescriptorType descriptor_type)
{
    if (ubos.find(name) != ubos.end()) {
        return true;
    }

    UBO ubo{};
    ubo.size = size;
    ubo.vecsize = vecsize;
    ubo.binding = binding;
    ubo.descriptor_type = descriptor_type;
    //ubo.cpu_buf = std::make_shared<char[]>(size * vecsize);
    ubo.gpu_bufs.reserve(swapchain_images.size());
    ubo.memos.reserve(swapchain_images.size());
    ubo.descriptors.reserve(swapchain_images.size());

    for (size_t i = 0; i < swapchain_images.size(); ++i) {
        vk::raii::Buffer gpu_buf{nullptr};
        vk::raii::DeviceMemory memo{nullptr};
        std::tie(gpu_buf, memo) = create_buffer(
            size * vecsize,
            usage,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        ubo.descriptors.push_back(vk::DescriptorBufferInfo{*gpu_buf, 0, size * vecsize});
        ubo.gpu_bufs.push_back(std::move(gpu_buf));
        ubo.memos.push_back(std::move(memo));
    }

    ubos.emplace(name, std::move(ubo));
    return true;
}

void Context::sync_uniform(const vk::raii::DeviceMemory& memo, const void* data, uint32_t size) const {
    void* mapped = memo.mapMemory(0, size);
    std::memcpy(mapped, data, size);
    memo.unmapMemory();
}

void Context::sync_ssbo(const SSBO& ssbo, const void* data, uint32_t swapchain_idx, uint32_t byte_size) const {
    if (data == nullptr || ssbo.gpu_bufs.empty() || swapchain_idx >= ssbo.gpu_bufs.size()) {
        return;
    }

    const auto capacity = static_cast<uint32_t>(ssbo.size * ssbo.vecsize);
    const auto upload_size = byte_size == 0 ? capacity : std::min(byte_size, capacity);
    if (upload_size == 0) {
        return;
    }

    auto [staging_buf, staging_memo] = create_buffer(
        upload_size,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    void* mapped = staging_memo.mapMemory(0, upload_size);
    std::memcpy(mapped, data, upload_size);
    staging_memo.unmapMemory();

    auto cmd_buf = begin_single_commands();
    vk::BufferCopy region{};
    region.srcOffset = 0;
    region.dstOffset = 0;
    region.size = upload_size;
    cmd_buf.copyBuffer(*staging_buf, *ssbo.gpu_bufs[swapchain_idx], region);
    end_single_commands(std::move(cmd_buf));
}

bool Context::create_pipeline_ssbo_gpu(Pipeline& pipeline, SSBO& ssbo) {
    if (ssbo.uses_borrowed_descriptors || ssbo.size == 0 || ssbo.vecsize == 0) {
        return false;
    }

    ssbo.gpu_bufs.clear();
    ssbo.memos.clear();
    ssbo.descriptors.clear();

    const vk::DeviceSize buffer_size = static_cast<vk::DeviceSize>(ssbo.size) * ssbo.vecsize;
    ssbo.gpu_bufs.reserve(swapchain_images.size());
    ssbo.memos.reserve(swapchain_images.size());
    ssbo.descriptors.reserve(swapchain_images.size());
    for (size_t i = 0; i < swapchain_images.size(); ++i) {
        vk::raii::Buffer gpu_buf{nullptr};
        vk::raii::DeviceMemory memo{nullptr};
        std::tie(gpu_buf, memo) = create_buffer(
            buffer_size,
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        ssbo.descriptors.push_back(vk::DescriptorBufferInfo{*gpu_buf, 0, buffer_size});
        ssbo.gpu_bufs.push_back(std::move(gpu_buf));
        ssbo.memos.push_back(std::move(memo));
    }

    update_pipeline_ssbo_descriptors(pipeline, ssbo);
    return true;
}

void Context::update_pipeline_ssbo_descriptors(Pipeline& pipeline, const SSBO& ssbo) {
    const auto& descriptors = ssbo.uses_borrowed_descriptors
        ? ssbo.borrowed_descriptors
        : ssbo.descriptors;
    if (descriptors.empty() || pipeline.descriptor_sets.size() != descriptors.size()) {
        return;
    }

    std::vector<vk::WriteDescriptorSet> writes;
    std::vector<vk::DescriptorBufferInfo> buffer_infos;
    writes.reserve(pipeline.descriptor_sets.size());
    buffer_infos.reserve(pipeline.descriptor_sets.size());
    for (size_t i = 0; i < pipeline.descriptor_sets.size(); ++i) {
        buffer_infos.push_back(descriptors[i]);
        vk::WriteDescriptorSet write{};
        write.dstSet = *pipeline.descriptor_sets[i];
        write.dstBinding = ssbo.binding;
        write.descriptorCount = 1;
        write.descriptorType = vk::DescriptorType::eStorageBuffer;
        write.pBufferInfo = &buffer_infos.back();
        writes.push_back(write);
    }
    if (!writes.empty()) {
        device.updateDescriptorSets(writes, {});
    }
}

bool Context::create_compute_ssbo_gpu(ComputePipeline& pipeline, SSBO& ssbo) {
    if (ssbo.size == 0 || ssbo.vecsize == 0) {
        return false;
    }

    ssbo.gpu_bufs.clear();
    ssbo.memos.clear();
    ssbo.descriptors.clear();

    const vk::DeviceSize buffer_size = static_cast<vk::DeviceSize>(ssbo.size) * ssbo.vecsize;
    ssbo.gpu_bufs.reserve(swapchain_images.size());
    ssbo.memos.reserve(swapchain_images.size());
    ssbo.descriptors.reserve(swapchain_images.size());
    for (size_t i = 0; i < swapchain_images.size(); ++i) {
        vk::raii::Buffer gpu_buf{nullptr};
        vk::raii::DeviceMemory memo{nullptr};
        std::tie(gpu_buf, memo) = create_buffer(
            buffer_size,
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        ssbo.descriptors.push_back(vk::DescriptorBufferInfo{*gpu_buf, 0, buffer_size});
        ssbo.gpu_bufs.push_back(std::move(gpu_buf));
        ssbo.memos.push_back(std::move(memo));
    }

    update_compute_ssbo_descriptors(pipeline, ssbo);
    return true;
}

void Context::update_compute_ssbo_descriptors(ComputePipeline& pipeline, const SSBO& ssbo) {
    if (ssbo.gpu_bufs.empty() || pipeline.descriptor_sets.size() != ssbo.gpu_bufs.size()) {
        return;
    }

    std::vector<vk::WriteDescriptorSet> writes;
    std::vector<vk::DescriptorBufferInfo> buffer_infos;
    writes.reserve(pipeline.descriptor_sets.size());
    buffer_infos.reserve(pipeline.descriptor_sets.size());
    for (size_t i = 0; i < pipeline.descriptor_sets.size(); ++i) {
        buffer_infos.push_back(ssbo.descriptors[i]);
        vk::WriteDescriptorSet write{};
        write.dstSet = *pipeline.descriptor_sets[i];
        write.dstBinding = ssbo.binding;
        write.descriptorCount = 1;
        write.descriptorType = vk::DescriptorType::eStorageBuffer;
        write.pBufferInfo = &buffer_infos.back();
        writes.push_back(write);
    }
    if (!writes.empty()) {
        device.updateDescriptorSets(writes, {});
    }
}

bool Context::alloc_pipeline_ssbo(const std::string& pipeline_name) {
    return alloc_pipeline_ssbo(pipeline_name, SSBOType_InstanceAttrs);
}

bool Context::alloc_pipeline_ssbo(const std::string& pipeline_name, SSBOType type) {
    const auto pipeline_it = pipelines.find(pipeline_name);
    if (pipeline_it == pipelines.end()) {
        return false;
    }

    auto& pipeline = pipeline_it->second;
    const auto ssbo_it = pipeline.ssbos.find(type);
    if (ssbo_it == pipeline.ssbos.end()) {
        return false;
    }

    auto& ssbo = ssbo_it->second;
    if (ssbo.uses_borrowed_descriptors) {
        return false;
    }
    if (!ssbo.gpu_bufs.empty()) {
        return true;
    }

    return create_pipeline_ssbo_gpu(pipeline, ssbo);
}

bool Context::resize_pipeline_ssbo(const std::string& pipeline_name, size_t new_vecsize) {
    return resize_pipeline_ssbo(pipeline_name, SSBOType_InstanceAttrs, new_vecsize);
}

bool Context::resize_pipeline_ssbo(const std::string& pipeline_name, SSBOType type, size_t new_vecsize) {
    if (new_vecsize == 0) {
        return false;
    }

    const auto pipeline_it = pipelines.find(pipeline_name);
    if (pipeline_it == pipelines.end()) {
        return false;
    }

    auto& pipeline = pipeline_it->second;
    const auto ssbo_it = pipeline.ssbos.find(type);
    if (ssbo_it == pipeline.ssbos.end()) {
        return false;
    }

    auto& ssbo = ssbo_it->second;
    if (ssbo.uses_borrowed_descriptors) {
        return false;
    }
    if (ssbo.vecsize == new_vecsize && !ssbo.gpu_bufs.empty()) {
        return true;
    }

    ssbo.vecsize = new_vecsize;
    if (ssbo.gpu_bufs.empty()) {
        return true;
    }

    return create_pipeline_ssbo_gpu(pipeline, ssbo);
}

bool Context::bind_pipeline_ssbo_from_compute(const std::string& graphics_pipeline_name,
    SSBOType graphics_type, const std::string& compute_pipeline_name, SSBOType compute_type)
{
    const auto graphics_pipeline_it = pipelines.find(graphics_pipeline_name);
    if (graphics_pipeline_it == pipelines.end()) {
        return false;
    }

    auto& graphics_pipeline = graphics_pipeline_it->second;
    const auto graphics_ssbo_it = graphics_pipeline.ssbos.find(graphics_type);
    if (graphics_ssbo_it == graphics_pipeline.ssbos.end()) {
        return false;
    }

    const auto compute_ssbo_name = compute_ssbo_full_name(compute_pipeline_name, compute_type);
    if (compute_ssbo_name.empty()) {
        return false;
    }

    try {
        const auto& compute_ssbo = require_compute_ssbo(compute_ssbo_name);
        if (compute_ssbo.descriptors.empty()
            || compute_ssbo.descriptors.size() != graphics_pipeline.descriptor_sets.size())
        {
            return false;
        }

        auto& graphics_ssbo = graphics_ssbo_it->second;
        graphics_ssbo.gpu_bufs.clear();
        graphics_ssbo.memos.clear();
        graphics_ssbo.descriptors.clear();
        graphics_ssbo.size = compute_ssbo.size;
        graphics_ssbo.vecsize = compute_ssbo.vecsize;
        graphics_ssbo.borrowed_descriptors = compute_ssbo.descriptors;
        graphics_ssbo.uses_borrowed_descriptors = true;
        update_pipeline_ssbo_descriptors(graphics_pipeline, graphics_ssbo);
        return true;
    }
    catch (const std::runtime_error&) {
        return false;
    }
}

bool Context::bind_pipeline_ssbo_from_mesh(const std::string& pipeline_name, const std::string& mesh_name) {
    const auto pipeline_it = pipelines.find(pipeline_name);
    if (pipeline_it == pipelines.end()) {
        return false;
    }

    const auto mesh_it = meshes.find(mesh_name);
    if (mesh_it == meshes.end()) {
        return false;
    }

    auto& pipeline = pipeline_it->second;
    const auto& mesh = mesh_it->second;
    if (!*mesh.vbuf || !*mesh.ibuf || mesh.vert_bytes == 0 || mesh.index_bytes == 0
        || pipeline.descriptor_sets.empty())
    {
        return false;
    }

    const auto bind_mesh_buffer = [&](SSBOType type, const vk::raii::Buffer& buf, vk::DeviceSize bytes) {
        const auto ssbo_it = pipeline.ssbos.find(type);
        if (ssbo_it == pipeline.ssbos.end()) {
            return false;
        }

        auto& ssbo = ssbo_it->second;
        ssbo.gpu_bufs.clear();
        ssbo.memos.clear();
        ssbo.descriptors.clear();
        ssbo.size = static_cast<uint32_t>(bytes);
        ssbo.vecsize = 1;
        ssbo.uses_borrowed_descriptors = true;
        ssbo.borrowed_descriptors.assign(
            pipeline.descriptor_sets.size(),
            vk::DescriptorBufferInfo{*buf, 0, bytes});
        update_pipeline_ssbo_descriptors(pipeline, ssbo);
        return true;
    };

    return bind_mesh_buffer(SSBOType_Vertices, mesh.vbuf, mesh.vert_bytes)
        && bind_mesh_buffer(SSBOType_Indices, mesh.ibuf, mesh.index_bytes);
}

bool Context::alloc_compute_ssbo(const std::string& full_name) {
    const auto split_pos = full_name.find(':');
    if (split_pos == std::string::npos) {
        return false;
    }

    const auto pipeline_it = compute_pipelines.find(full_name.substr(0, split_pos));
    if (pipeline_it == compute_pipelines.end()) {
        return false;
    }

    auto& pipeline = pipeline_it->second;
    const auto ssbo_it = pipeline.ssbos.find(full_name);
    if (ssbo_it == pipeline.ssbos.end()) {
        return false;
    }

    auto& ssbo = ssbo_it->second;
    return !ssbo.gpu_bufs.empty() || create_compute_ssbo_gpu(pipeline, ssbo);
}

bool Context::resize_compute_ssbo(const std::string& full_name, size_t new_vecsize) {
    if (new_vecsize == 0) {
        return false;
    }

    const auto split_pos = full_name.find(':');
    if (split_pos == std::string::npos) {
        return false;
    }

    const auto pipeline_it = compute_pipelines.find(full_name.substr(0, split_pos));
    if (pipeline_it == compute_pipelines.end()) {
        return false;
    }

    auto& pipeline = pipeline_it->second;
    const auto ssbo_it = pipeline.ssbos.find(full_name);
    if (ssbo_it == pipeline.ssbos.end()) {
        return false;
    }

    auto& ssbo = ssbo_it->second;
    if (ssbo.vecsize == new_vecsize && !ssbo.gpu_bufs.empty()) {
        return true;
    }

    ssbo.vecsize = new_vecsize;
    return ssbo.gpu_bufs.empty() || create_compute_ssbo_gpu(pipeline, ssbo);
}

bool Context::sync_compute_ssbo(const std::string& full_name, const void* data,
    uint32_t swapchain_idx, uint32_t byte_size) const
{
    try {
        sync_ssbo(require_compute_ssbo(full_name), data, swapchain_idx, byte_size);
        return true;
    }
    catch (const std::runtime_error&) {
        return false;
    }
}

bool Context::alloc_compute_ssbo(const std::string& pipeline_name, SSBOType type) {
    const auto full_name = compute_ssbo_full_name(pipeline_name, type);
    return !full_name.empty() && alloc_compute_ssbo(full_name);
}

bool Context::resize_compute_ssbo(const std::string& pipeline_name, SSBOType type, size_t new_vecsize) {
    const auto full_name = compute_ssbo_full_name(pipeline_name, type);
    return !full_name.empty() && resize_compute_ssbo(full_name, new_vecsize);
}

bool Context::sync_compute_ssbo(const std::string& pipeline_name, SSBOType type, const void* data,
    uint32_t swapchain_idx, uint32_t byte_size) const
{
    const auto full_name = compute_ssbo_full_name(pipeline_name, type);
    return !full_name.empty() && sync_compute_ssbo(full_name, data, swapchain_idx, byte_size);
}

UBO& Context::require_ubo(const std::string& full_name) {
    const auto split_pos = full_name.find(':');
    if (split_pos != std::string::npos) {
        const auto pipeline_name = full_name.substr(0, split_pos);
        const auto reflected_name = full_name.substr(split_pos + 1);
        const auto pipeline_found = pipelines.find(pipeline_name);
        if (pipeline_found != pipelines.end()) {
            UBOType ubo_type{};
            if (resolve_ubo_type(reflected_name, ubo_type)) {
                const auto ubo_found = pipeline_found->second.ubos.find(ubo_type);
                if (ubo_found != pipeline_found->second.ubos.end()) {
                    return ubo_found->second;
                }
            }
        }
    }

    for (auto& [pipeline_name, pipeline] : compute_pipelines) {
        (void)pipeline_name;
        const auto ubo_found = pipeline.ubos.find(full_name);
        if (ubo_found != pipeline.ubos.end()) {
            return ubo_found->second;
        }
    }

    throw std::runtime_error("ubo not found: " + full_name);
}

const SSBO& Context::require_compute_ssbo(const std::string& full_name) const {
    const auto split_pos = full_name.find(':');
    if (split_pos != std::string::npos) {
        const auto pipeline_it = compute_pipelines.find(full_name.substr(0, split_pos));
        if (pipeline_it != compute_pipelines.end()) {
            const auto ssbo_it = pipeline_it->second.ssbos.find(full_name);
            if (ssbo_it != pipeline_it->second.ssbos.end()) {
                return ssbo_it->second;
            }
        }
    }

    throw std::runtime_error("compute ssbo not found: " + full_name);
}

bool Context::load_mesh(const std::string& name, const Mesh& mesh) {
    MeshGPU gpu{};
    gpu.sync(mesh, this);
    meshes.emplace(name, std::move(gpu));
    return true;
}

bool Context::add_render_target(vk::ImageUsageFlags usage, vk::Format format,
    uint32_t width, uint32_t height, vk::ImageLayout layout, vk::SampleCountFlagBits samples)
{
    Texture target{};
    target.binding = static_cast<uint32_t>(targets.size());
    target.vecsize = 1;

    const uint32_t target_width = width == 0 ? swapchain_extent.width : width;
    const uint32_t target_height = height == 0 ? swapchain_extent.height : height;
    if (target_width == 0 || target_height == 0) {
        std::cout << "Render target dimensions must be non-zero" << std::endl;
        return false;
    }
    target.width = target_width;
    target.height = target_height;

    std::tie(target.image, target.memo) = create_vk_image(
        target_width, target_height, 1, samples, format, vk::ImageTiling::eOptimal,
        usage, vk::MemoryPropertyFlagBits::eDeviceLocal);

    vk::ImageAspectFlags aspect_mask = vk::ImageAspectFlagBits::eColor;
    if (format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint) {
        aspect_mask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
    }
    else if (format == vk::Format::eD16Unorm || format == vk::Format::eD32Sfloat
        || format == vk::Format::eX8D24UnormPack32)
    {
        aspect_mask = vk::ImageAspectFlagBits::eDepth;
    }

    target.view = create_vk_imageview(target.image, format, aspect_mask);
    target.sampler = create_vk_sampler();
    target.layout = layout;
    target.descriptor = vk::DescriptorImageInfo{*target.sampler, *target.view, target.layout};

    targets.emplace_back(std::move(target));
    return true;
}

bool Context::set_render_target(uint32_t target_index) {
    if (target_index >= targets.size()) {
        std::cout << "Render target index out of range: " << target_index << std::endl;
        return false;
    }
    active_render_target_index_ = static_cast<int32_t>(target_index);
    return true;
}

void Context::set_render_to_framebuffer() {
    active_render_target_index_ = -1;
}

bool Context::add_texture(const std::string& name, const uint32_t binding,
    const fs::path& path) {
    if (textures.find(name) != textures.end()) {
        std::cout << "Texture " << name << " already exists" << std::endl;
        return false;
    }

    Texture tex{};
    tex.binding = binding;
    tex.vecsize = 1;
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
    tex.width = static_cast<uint32_t>(spec.width);
    tex.height = static_cast<uint32_t>(spec.height);
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
        vk::MemoryPropertyFlagBits::eDeviceLocal);

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

bool Context::add_cubemap(const std::string& name, const uint32_t binding,
    const fs::path& path)
{
    if (textures.find(name) != textures.end()) {
        std::cout << "Texture " << name << " already exists" << std::endl;
        return false;
    }

    Texture tex{};
    tex.binding = binding;
    tex.vecsize = 6;

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
    tex.width = face_size;
    tex.height = face_size;

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
        vk::MemoryPropertyFlagBits::eDeviceLocal, vk::ImageCreateFlagBits::eCubeCompatible);

    vk::raii::CommandBuffer cmd_buf = begin_single_commands();

    transit_image_layout(cmd_buf, tex.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, 6);

    std::vector<vk::BufferImageCopy> regions;
    regions.reserve(6);
    vk::DeviceSize offset = 0;
    const vk::DeviceSize face_bytes = static_cast<vk::DeviceSize>(face_size) * face_size * sizeof(float);
    for (uint32_t layer = 0; layer < 6; ++layer) {
        vk::BufferImageCopy region{};
        region.bufferOffset = offset;
        region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = layer;
        region.imageSubresource.layerCount = 1;
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

void Context::transit_image_layout(vk::raii::CommandBuffer& cmd_buf, const vk::raii::Image& img, vk::ImageLayout old_layout, vk::ImageLayout new_layout, const uint32_t layer_count) const {
    vk::ImageMemoryBarrier barrier{};
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.image = img;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = layer_count;
    vk::PipelineStageFlags src_stage;
    vk::PipelineStageFlags dst_stage;

    if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        src_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        dst_stage = vk::PipelineStageFlagBits::eTransfer;
    }
    else if (old_layout == vk::ImageLayout::eTransferDstOptimal && new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        src_stage = vk::PipelineStageFlagBits::eTransfer;
        dst_stage = vk::PipelineStageFlagBits::eFragmentShader;
    }
    else {
        throw std::runtime_error("unsupported image layout transition");
    }
    cmd_buf.pipelineBarrier(src_stage, dst_stage, {}, {}, {}, barrier);
}

void Context::copy_buffer_to_image(vk::raii::CommandBuffer& cmd_buf, const vk::raii::Buffer& buf, const vk::raii::Image& img, uint32_t width, uint32_t height) const {
    vk::BufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = vk::Offset3D{0, 0, 0};
    region.imageExtent = vk::Extent3D{width, height, 1};
    cmd_buf.copyBufferToImage(buf, img, vk::ImageLayout::eTransferDstOptimal, region);
}

std::pair<vk::raii::Image, vk::raii::DeviceMemory> Context::create_vk_image(uint32_t width, uint32_t height, uint32_t layers, vk::SampleCountFlagBits samples, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::ImageCreateFlags flags) const {
    vk::ImageCreateInfo image_create_info{};
    image_create_info.imageType = vk::ImageType::e2D;
    image_create_info.format = format;
    image_create_info.extent = vk::Extent3D{width, height, 1};
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = layers;
    image_create_info.samples = samples;
    image_create_info.tiling = tiling;
    image_create_info.usage = usage;
    image_create_info.sharingMode = vk::SharingMode::eExclusive;
    image_create_info.flags = flags;
    vk::raii::Image image = vk::raii::Image(device, image_create_info);
    vk::MemoryRequirements mem_reqs = image.getMemoryRequirements();
    vk::MemoryAllocateInfo alloc_info{};
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits, properties);
    vk::raii::DeviceMemory memo = vk::raii::DeviceMemory(device, alloc_info);
    image.bindMemory(memo, 0);
    return {std::move(image), std::move(memo)};
}

vk::raii::ImageView Context::create_vk_imageview(vk::Image img, vk::Format format, vk::ImageAspectFlags aspect_mask) const {
    vk::ImageViewCreateInfo view_info{};
    view_info.image = img;
    view_info.viewType = vk::ImageViewType::e2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = aspect_mask;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.layerCount = 1;
    return vk::raii::ImageView(device, view_info);
}

vk::raii::ImageView Context::create_vk_imageview(const vk::raii::Image& img, vk::Format format, uint32_t layer_count,
    vk::ImageAspectFlags aspect_mask) const
{
    vk::ImageViewCreateInfo view_info{};
    view_info.image = *img;
    view_info.viewType = layer_count == 1 ? vk::ImageViewType::e2D : vk::ImageViewType::eCube;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = aspect_mask;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.layerCount = layer_count;
    return vk::raii::ImageView(device, view_info);
}

vk::raii::Sampler Context::create_vk_sampler(vk::Filter mag_filter, vk::Filter min_filter, vk::SamplerMipmapMode mipmap_mode, vk::SamplerAddressMode address_mode, bool anisotropy_enable, bool compare_enable, vk::CompareOp compare_op) const {
    vk::PhysicalDeviceProperties props = physical_device.getProperties();
    vk::SamplerCreateInfo sampler_info{};
    sampler_info.magFilter = mag_filter;
    sampler_info.minFilter = min_filter;
    sampler_info.mipmapMode = mipmap_mode;
    sampler_info.addressModeU = address_mode;
    sampler_info.addressModeV = address_mode;
    sampler_info.addressModeW = address_mode;
    sampler_info.mipLodBias = 0.f;
    sampler_info.anisotropyEnable = anisotropy_enable;
    sampler_info.maxAnisotropy = props.limits.maxSamplerAnisotropy;
    sampler_info.compareEnable = compare_enable;
    sampler_info.compareOp = compare_op;
    return vk::raii::Sampler(device, sampler_info);
}

}
