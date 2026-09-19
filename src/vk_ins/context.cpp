#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <ranges>
#include <stdexcept>
#include <system_error>
#include <vector>

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

#include "asset_mgr/light_mgr.h"
#include "vk_ins/context.hpp"

namespace vkkk
{

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
    const vk::DescriptorSet* desc_set, uint32_t index_count) const
{
    cmd_buf.bindVertexBuffers(0, *vbuf, {0});
    if (desc_set != nullptr) {
        cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, ppl_layout, 0, {*desc_set}, {});
    }
    cmd_buf.bindIndexBuffer(*ibuf, 0, vk::IndexType::eUint32);
    cmd_buf.drawIndexed(index_count == 0 ? icnt * 3 : index_count, 1, 0, 0, 0);
}

void MeshGPU::emit_draw_cmd_instanced(vk::CommandBuffer cmd_buf, vk::PipelineLayout ppl_layout,
    uint32_t instance_count, uint32_t offset, const vk::DescriptorSet* desc_set,
    uint32_t index_count) const
{
    cmd_buf.bindVertexBuffers(0, *vbuf, {0});
    if (desc_set != nullptr) {
        cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, ppl_layout, 0, {*desc_set}, {});
    }
    cmd_buf.bindIndexBuffer(*ibuf, 0, vk::IndexType::eUint32);
    cmd_buf.drawIndexed(index_count == 0 ? icnt * 3 : index_count, instance_count, 0, 0, offset);
}

void LinesGPU::sync(const Lines& lines, Context* ctx) {
    if (!lines.loaded || lines.vcnt == 0 || lines.icnt == 0 || lines.icnt % 2 != 0) {
        throw std::runtime_error("cannot sync unloaded or invalid line-list data");
    }

    ctx->create_vertex_buffer(lines.vbuf.get(), vbuf, vbuf_memo, lines.comp_size, lines.vcnt);
    ctx->create_index_buffer(lines.ibuf.get(), ibuf, ibuf_memo, lines.icnt);
    vcnt = lines.vcnt;
    icnt = lines.icnt;
    vert_bytes = static_cast<vk::DeviceSize>(lines.comp_size) * lines.vcnt * sizeof(float);
}

void LinesGPU::emit_draw_cmd(vk::CommandBuffer cmd_buf, uint32_t index_count, uint32_t instance_count,
    uint32_t instance_offset) const
{
    cmd_buf.bindVertexBuffers(0, *vbuf, {0});
    cmd_buf.bindIndexBuffer(*ibuf, 0, vk::IndexType::eUint32);
    cmd_buf.drawIndexed(index_count == 0 ? icnt : index_count, instance_count, 0, 0, instance_offset);
}

void PointsGPU::sync(const Points& points, Context* ctx) {
    if (!points.loaded || points.vcnt == 0) {
        throw std::runtime_error("cannot sync unloaded or invalid point-list data");
    }

    ctx->create_vertex_buffer(points.vbuf.get(), vbuf, vbuf_memo, points.comp_size, points.vcnt);
    vcnt = points.vcnt;
    vert_bytes = static_cast<vk::DeviceSize>(points.comp_size) * points.vcnt * sizeof(float);
}

void PointsGPU::emit_draw_cmd(vk::CommandBuffer cmd_buf, uint32_t vertex_count,
    uint32_t instance_count, uint32_t instance_offset) const
{
    cmd_buf.bindVertexBuffers(0, *vbuf, {0});
    cmd_buf.draw(vertex_count == 0 ? vcnt : vertex_count, instance_count, 0, instance_offset);
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

void Context::set_shader_cache(fs::path directory, bool force_recompile) {
    shader_cache_options_.directory = std::move(directory);
    shader_cache_options_.force_recompile = force_recompile;
}

bool Context::load_shader(ShaderModule& module, const fs::path& path,
    vk::ShaderStageFlagBits stage) const
{
    return module.load(path, stage, shader_cache_options_);
}

bool Context::load_shader(ShaderModule& module, const char* source,
    vk::ShaderStageFlagBits stage, const std::string& source_name) const
{
    return module.load(source, stage, source_name, shader_cache_options_);
}

void Context::set_pipeline_cache_path(fs::path path, bool force_recompile) {
    pipeline_cache_path_ = std::move(path);
    pipeline_cache_force_recompile_ = force_recompile;
}

bool Context::load_pipeline_cache(const fs::path& path) {
    if (device == nullptr) {
        return false;
    }

    std::vector<char> initial_data;
    if (!path.empty()) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (file) {
            const auto end = file.tellg();
            if (end > 0) {
                initial_data.resize(static_cast<std::size_t>(end));
                file.seekg(0);
                file.read(initial_data.data(),
                    static_cast<std::streamsize>(initial_data.size()));
                if (!file) {
                    initial_data.clear();
                }
            }
        }
    }

    vk::PipelineCacheCreateInfo create_info{};
    create_info.initialDataSize = initial_data.size();
    create_info.pInitialData = initial_data.empty()
        ? nullptr : initial_data.data();
    try {
        pipeline_cache = vk::raii::PipelineCache(device, create_info);
        return true;
    }
    catch (const std::exception& error) {
        if (!initial_data.empty()) {
            std::cerr << "vkkk: ignoring invalid pipeline cache '"
                      << path << "': " << error.what() << '\n';
            create_info.initialDataSize = 0;
            create_info.pInitialData = nullptr;
            try {
                pipeline_cache = vk::raii::PipelineCache(device, create_info);
                return true;
            }
            catch (const std::exception&) {
                // Fall through and report a hard cache creation failure.
            }
        }
        std::cerr << "vkkk: failed to create pipeline cache: "
                  << error.what() << '\n';
        return false;
    }
}

bool Context::save_pipeline_cache(const fs::path& path) const {
    if (path.empty() || pipeline_cache == nullptr) {
        return false;
    }

    const auto data = pipeline_cache.getData();
    if (data.empty()) {
        return false;
    }

    std::error_code error;
    if (!path.parent_path().empty()) {
        fs::create_directories(path.parent_path(), error);
        if (error) {
            return false;
        }
    }
    const fs::path temporary = path.string() + ".tmp";
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file) {
            return false;
        }
        file.write(reinterpret_cast<const char*>(data.data()),
            static_cast<std::streamsize>(data.size()));
        file.flush();
        if (!file) {
            file.close();
            fs::remove(temporary, error);
            return false;
        }
    }
    fs::remove(path, error);
    error.clear();
    fs::rename(temporary, path, error);
    if (error) {
        fs::remove(temporary, error);
        return false;
    }
    return true;
}

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
    const VkExtent2D framebuffer_extent = window_backend != nullptr
        ? window_backend->framebuffer_size()
        : VkExtent2D{swapchain_extent.width, swapchain_extent.height};
    swapchain_extent = choose_swap_extent(surface_capabilities, framebuffer_extent);
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
    swapchain_image_layouts.assign(swapchain_images.size(), vk::ImageLayout::eUndefined);
}

void Context::create_imageviews() {
    swapchain_image_views.clear();
    swapchain_image_views.reserve(swapchain_images.size());
    for (const auto& image : swapchain_images) {
        swapchain_image_views.emplace_back(create_vk_imageview(image, swapchain_surface_format.format));
    }
}

void Context::recreate_swapchain() {
    if (window_backend != nullptr) {
        if (window_backend->should_close()) {
            return;
        }
        window_backend->wait_until_visible();
        if (window_backend->should_close()) {
            return;
        }
    }
    device.waitIdle();

    swapchain_image_views.clear();
    depth_view = nullptr;
    depth_image = nullptr;
    depth_memo = nullptr;
    // Keep custom render targets / depth attachments (shadow maps, HDR, etc.).
    // Swapchain-sized ones are resized below; fixed-size ones (e.g. shadow) survive as-is.
    swapchain = nullptr;

    try {
        create_swapchain();
    }
    catch (const vk::SurfaceLostKHRError&) {
        return;
    }
    create_imageviews();
    create_depth_resources();
    images_in_flight.assign(swapchain_images.size(), VK_NULL_HANDLE);
    render_finished_semaphores.clear();
    render_finished_semaphores.reserve(swapchain_images.size());
    for (size_t i = 0; i < swapchain_images.size(); ++i) {
        render_finished_semaphores.emplace_back(device, vk::SemaphoreCreateInfo{});
    }

    vk::CommandBufferAllocateInfo cmd_buf_alloc_info{};
    cmd_buf_alloc_info.commandPool = command_pool;
    cmd_buf_alloc_info.level = vk::CommandBufferLevel::ePrimary;
    cmd_buf_alloc_info.commandBufferCount = static_cast<uint32_t>(swapchain_images.size());
    command_buffers = vk::raii::CommandBuffers(device, cmd_buf_alloc_info);

    for (uint32_t i = 0; i < static_cast<uint32_t>(targets.size()); ++i) {
        if (targets[i].matchSwapchain) {
            resize_render_target(i, swapchain_extent.width, swapchain_extent.height);
        }
    }
    for (uint32_t i = 0; i < static_cast<uint32_t>(depth_attachments.size()); ++i) {
        if (depth_attachments[i].matchSwapchain) {
            resize_depth_attachment(i, swapchain_extent.width, swapchain_extent.height);
        }
    }

    if (resize_cbk_) {
        resize_cbk_(swapchain_extent.width, swapchain_extent.height);
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
    vk::MemoryPropertyFlags properties,
    bool exportable) const
{
    const bool export_memory = exportable && external_memory_export_available;
#ifdef _WIN32
    const vk::ExternalMemoryHandleTypeFlagBits handle_type =
        vk::ExternalMemoryHandleTypeFlagBits::eOpaqueWin32;
#else
    const vk::ExternalMemoryHandleTypeFlagBits handle_type =
        vk::ExternalMemoryHandleTypeFlagBits::eOpaqueFd;
#endif

    vk::BufferCreateInfo buffer_create_info{};
    buffer_create_info.size = size;
    buffer_create_info.usage = usage;
    buffer_create_info.sharingMode = vk::SharingMode::eExclusive;
    vk::ExternalMemoryBufferCreateInfo external_buffer_info{};
    if (export_memory) {
        external_buffer_info.handleTypes = handle_type;
        buffer_create_info.pNext = &external_buffer_info;
    }
    vk::raii::Buffer buffer = vk::raii::Buffer(device, buffer_create_info);
    vk::MemoryRequirements mem_reqs = buffer.getMemoryRequirements();
    vk::MemoryAllocateInfo alloc_info{};
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits, properties);
    vk::ExportMemoryAllocateInfo export_info{};
    vk::MemoryDedicatedAllocateInfo dedicated_info{};
    if (export_memory) {
        export_info.handleTypes = handle_type;
        dedicated_info.buffer = *buffer;
        dedicated_info.pNext = &export_info;
        alloc_info.pNext = &dedicated_info;
    }
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

void Context::init(WindowBackend& backend,
    const char* app_name,
    uint32_t app_version,
    const char* engine_name,
    uint32_t api_version,
    bool enable_validation_layers,
    const std::vector<const char*>& extra_validation_layers,
    const std::vector<const char*>& extra_extensions)
{
    window_backend = &backend;
    window_backend->set_resize_flag(&frame_buffer_resized);

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

    std::vector<const char*> instance_extensions = window_backend->instance_extensions(enable_validation_layers);
    for (const char* extra : extra_extensions) {
        const bool already_present = std::ranges::any_of(instance_extensions, [extra](const char* existing) {
            return std::strcmp(existing, extra) == 0;
        });
        if (!already_present) {
            instance_extensions.push_back(extra);
        }
    }
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
    VkSurfaceKHR raw_surface = window_backend->create_surface(static_cast<VkInstance>(*instance));
    if (raw_surface == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to create window surface");
    }
    surface = vk::raii::SurfaceKHR(instance, raw_surface);

    std::vector<const char*> required_device_extensions = required_extensions;
    if (use_mesh_shader) {
        required_device_extensions.push_back(vk::EXTMeshShaderExtensionName);
    }

#ifdef _WIN32
    const char* external_memory_ext = "VK_KHR_external_memory_win32";
#else
    const char* external_memory_ext = "VK_KHR_external_memory_fd";
#endif
    std::vector<vk::raii::PhysicalDevice> devices = instance.enumeratePhysicalDevices();
    const auto supports_external_memory = [&](const auto& device) {
        const auto extensions = device.enumerateDeviceExtensionProperties();
        return std::ranges::any_of(extensions, [external_memory_ext](const auto& available) {
            return std::strcmp(available.extensionName, external_memory_ext) == 0;
        });
    };
    auto dev_iter = std::ranges::find_if(devices, [&](const auto& device) {
        if (device.getProperties().deviceType != vk::PhysicalDeviceType::eDiscreteGpu
            || !supports_external_memory(device))
        {
            return false;
        }
        auto candidate_extensions = required_device_extensions;
        candidate_extensions.push_back(external_memory_ext);
        return is_device_suitable(device, surface, candidate_extensions);
    });
    if (dev_iter == devices.end()) {
        dev_iter = std::ranges::find_if(devices, [&](const auto& device) {
            if (!supports_external_memory(device)) {
                return false;
            }
            auto candidate_extensions = required_device_extensions;
            candidate_extensions.push_back(external_memory_ext);
            return is_device_suitable(device, surface, candidate_extensions);
        });
    }
    if (dev_iter == devices.end()) {
        dev_iter = std::ranges::find_if(devices, [&](const auto& device) {
            return is_device_suitable(device, surface, required_device_extensions);
        });
    }
    if (dev_iter == devices.end()) {
        throw std::runtime_error("no suitable Vulkan 1.3 device with dynamic rendering support");
    }
    physical_device = *dev_iter;
    const auto selected_properties = physical_device.getProperties();
    std::cerr << "vkkk CUDA interop: selected Vulkan device '"
        << selected_properties.deviceName
        << "' vendor=0x" << std::hex << selected_properties.vendorID
        << " device=0x" << selected_properties.deviceID << std::dec
        << " external-memory="
        << (supports_external_memory(physical_device) ? "yes" : "no") << "\n";

    const auto device_extensions = physical_device.enumerateDeviceExtensionProperties();
    if (std::ranges::any_of(device_extensions, [external_memory_ext](const auto& available) {
            return std::strcmp(available.extensionName, external_memory_ext) == 0;
        }))
    {
        required_device_extensions.push_back(external_memory_ext);
        external_memory_export_available = true;
    }

    queue_idx = find_graphics_queue_family_index();
    if (queue_idx == ~0u) {
        throw std::runtime_error("no suitable queue family found");
    }
    compute_queue_idx = find_compute_queue_family_index(queue_idx);

    const vk::PhysicalDeviceFeatures supported_features = physical_device.getFeatures();
    sample_rate_shading_enabled = supported_features.sampleRateShading == vk::True;
    wide_lines_enabled = supported_features.wideLines == vk::True;
    large_points_enabled = supported_features.largePoints == vk::True;
    const bool shader_float64_supported = supported_features.shaderFloat64 == vk::True;
    const bool shader_int64_supported = supported_features.shaderInt64 == vk::True;
    point_size_range = physical_device.getProperties().limits.pointSizeRange;
    line_width_range = physical_device.getProperties().limits.lineWidthRange;
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
    device_features.get<vk::PhysicalDeviceFeatures2>().features.wideLines =
        wide_lines_enabled ? VK_TRUE : VK_FALSE;
    device_features.get<vk::PhysicalDeviceFeatures2>().features.largePoints =
        large_points_enabled ? VK_TRUE : VK_FALSE;
    // Vertex picking A-buffer writes storage buffers from the fragment stage.
    device_features.get<vk::PhysicalDeviceFeatures2>().features.fragmentStoresAndAtomics =
        supported_features.fragmentStoresAndAtomics;
    device_features.get<vk::PhysicalDeviceFeatures2>().features.fillModeNonSolid =
        supported_features.fillModeNonSolid;
    // Needed for draw_indirect with draw_count > 1.
    device_features.get<vk::PhysicalDeviceFeatures2>().features.multiDrawIndirect =
        supported_features.multiDrawIndirect;
    // The built-in joint/point/line shaders use double and int64_t values in
    // their storage-buffer layouts.  These core features must be explicitly
    // enabled before Vulkan shader modules declaring Float64/Int64 are created.
    // Enable them only when the selected physical device advertises support.
    device_features.get<vk::PhysicalDeviceFeatures2>().features.shaderFloat64 =
        shader_float64_supported ? VK_TRUE : VK_FALSE;
    device_features.get<vk::PhysicalDeviceFeatures2>().features.shaderInt64 =
        shader_int64_supported ? VK_TRUE : VK_FALSE;
    device_features.get<vk::PhysicalDeviceVulkan13Features>().synchronization2 = VK_TRUE;
    device_features.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering = VK_TRUE;
    // shaderc's Vulkan 1.3 target emits SPIR-V 1.6 LocalSizeId for mesh/task
    // local_size_*; that execution mode requires maintenance4.
    device_features.get<vk::PhysicalDeviceVulkan13Features>().maintenance4 = VK_TRUE;
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

    if (pipeline_cache_path_.empty() || pipeline_cache_force_recompile_) {
        if (!load_pipeline_cache({})) {
            throw std::runtime_error("failed to create Vulkan pipeline cache");
        }
    }
    else if (!load_pipeline_cache(pipeline_cache_path_)) {
        throw std::runtime_error("failed to create Vulkan pipeline cache");
    }

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

} // namespace vkkk
