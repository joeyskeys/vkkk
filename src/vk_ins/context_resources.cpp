#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>

#include "vk_ins/context.hpp"

namespace vkkk
{

namespace {

std::string compute_ssbo_full_name(const std::string& pipeline_name, const std::string& block_name) {
    return pipeline_name + ":" + block_name;
}

} // namespace

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

bool Context::sync_ubo(const std::string& pipeline_name, const std::string& block_name,
    const void* data, uint32_t frame_idx, uint32_t byte_size) const
{
    if (data == nullptr) {
        return false;
    }
    const auto pipeline_it = pipelines.find(pipeline_name);
    if (pipeline_it == pipelines.end()) {
        return false;
    }
    const auto ubo_it = pipeline_it->second.ubos.find(block_name);
    if (ubo_it == pipeline_it->second.ubos.end() || frame_idx >= ubo_it->second.memos.size()) {
        return false;
    }
    const auto& ubo = ubo_it->second;
    const auto capacity = static_cast<uint32_t>(ubo.size * ubo.vecsize);
    const auto upload_size = byte_size == 0 ? capacity : std::min(byte_size, capacity);
    if (upload_size == 0) {
        return false;
    }
    sync_uniform(ubo.memos[frame_idx], data, upload_size);
    return true;
}

bool Context::sync_ssbo(const std::string& pipeline_name, const std::string& block_name,
    const void* data, uint32_t frame_idx, uint32_t byte_size) const
{
    const auto pipeline_it = pipelines.find(pipeline_name);
    if (pipeline_it == pipelines.end()) {
        return false;
    }
    const auto ssbo_it = pipeline_it->second.ssbos.find(block_name);
    if (ssbo_it == pipeline_it->second.ssbos.end()) {
        return false;
    }
    sync_ssbo(ssbo_it->second, data, frame_idx, byte_size);
    return true;
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

    // Host-visible SSBOs: map directly. Staging + queueSubmit while another command
    // buffer from the same pool is still recording can leave storage contents stale.
    if (swapchain_idx < ssbo.memos.size()) {
        void* mapped = ssbo.memos[swapchain_idx].mapMemory(0, upload_size);
        std::memcpy(mapped, data, upload_size);
        ssbo.memos[swapchain_idx].unmapMemory();
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

bool Context::alloc_pipeline_ssbo(const std::string& pipeline_name, const std::string& block_name) {
    const auto pipeline_it = pipelines.find(pipeline_name);
    if (pipeline_it == pipelines.end()) {
        return false;
    }

    auto& pipeline = pipeline_it->second;
    const auto ssbo_it = pipeline.ssbos.find(block_name);
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

bool Context::resize_pipeline_ssbo(const std::string& pipeline_name, const std::string& block_name,
    size_t new_vecsize)
{
    if (new_vecsize == 0) {
        return false;
    }

    const auto pipeline_it = pipelines.find(pipeline_name);
    if (pipeline_it == pipelines.end()) {
        return false;
    }

    auto& pipeline = pipeline_it->second;
    const auto ssbo_it = pipeline.ssbos.find(block_name);
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
    const std::string& graphics_block_name, const std::string& compute_pipeline_name,
    const std::string& compute_block_name)
{
    const auto graphics_pipeline_it = pipelines.find(graphics_pipeline_name);
    if (graphics_pipeline_it == pipelines.end()) {
        return false;
    }

    auto& graphics_pipeline = graphics_pipeline_it->second;
    const auto graphics_ssbo_it = graphics_pipeline.ssbos.find(graphics_block_name);
    if (graphics_ssbo_it == graphics_pipeline.ssbos.end()) {
        return false;
    }

    const auto compute_ssbo_name = compute_ssbo_full_name(compute_pipeline_name, compute_block_name);
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

    const auto bind_mesh_buffer = [&](const char* block_name, const vk::raii::Buffer& buf,
        vk::DeviceSize bytes)
    {
        const auto ssbo_it = pipeline.ssbos.find(block_name);
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

    return bind_mesh_buffer(buf::Vertices, mesh.vbuf, mesh.vert_bytes)
        && bind_mesh_buffer(buf::Indices, mesh.ibuf, mesh.index_bytes);
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

bool Context::alloc_compute_ssbo(const std::string& pipeline_name, const std::string& block_name) {
    return alloc_compute_ssbo(compute_ssbo_full_name(pipeline_name, block_name));
}

bool Context::resize_compute_ssbo(const std::string& pipeline_name, const std::string& block_name,
    size_t new_vecsize)
{
    return resize_compute_ssbo(compute_ssbo_full_name(pipeline_name, block_name), new_vecsize);
}

bool Context::sync_compute_ssbo(const std::string& pipeline_name, const std::string& block_name,
    const void* data, uint32_t swapchain_idx, uint32_t byte_size) const
{
    return sync_compute_ssbo(compute_ssbo_full_name(pipeline_name, block_name), data, swapchain_idx,
        byte_size);
}

UBO& Context::require_ubo(const std::string& full_name) {
    const auto split_pos = full_name.find(':');
    if (split_pos != std::string::npos) {
        const auto pipeline_name = full_name.substr(0, split_pos);
        const auto reflected_name = full_name.substr(split_pos + 1);
        const auto pipeline_found = pipelines.find(pipeline_name);
        if (pipeline_found != pipelines.end()) {
            const auto ubo_found = pipeline_found->second.ubos.find(reflected_name);
            if (ubo_found != pipeline_found->second.ubos.end()) {
                return ubo_found->second;
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

bool Context::load_lines(const std::string& name, const Lines& line_data) {
    LinesGPU gpu{};
    gpu.sync(line_data, this);
    lines.emplace(name, std::move(gpu));
    return true;
}

uint32_t Context::add_render_target(vk::ImageUsageFlags usage, vk::Format format,
    uint32_t width, uint32_t height, vk::ImageLayout layout, vk::SampleCountFlagBits samples)
{
    Texture target{};
    target.binding = static_cast<uint32_t>(targets.size());
    target.vecsize = 1;
    target.matchSwapchain = (width == 0 || height == 0);
    target.samples = samples;

    const uint32_t target_width = width == 0 ? swapchain_extent.width : width;
    const uint32_t target_height = height == 0 ? swapchain_extent.height : height;
    if (target_width == 0 || target_height == 0) {
        std::cout << "Render target dimensions must be non-zero" << std::endl;
        return kInvalidTargetIndex;
    }
    target.width = target_width;
    target.height = target_height;
    target.format = format;
    target.usage = usage;

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
    target.layout = vk::ImageLayout::eUndefined;
    target.descriptor = vk::DescriptorImageInfo{*target.sampler, *target.view, layout};

    const uint32_t index = static_cast<uint32_t>(targets.size());
    targets.emplace_back(std::move(target));
    return index;
}

uint32_t Context::create_rgba8_render_target(const uint8_t* pixels, uint32_t width, uint32_t height,
    size_t byte_size)
{
    if (pixels == nullptr || width == 0 || height == 0
        || static_cast<size_t>(width) > std::numeric_limits<size_t>::max() / height / 4)
    {
        return kInvalidTargetIndex;
    }
    const size_t required_size = static_cast<size_t>(width) * height * 4;
    if (byte_size != required_size || required_size > std::numeric_limits<uint32_t>::max()) {
        return kInvalidTargetIndex;
    }

    const uint32_t target_index = add_render_target(
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
            | vk::ImageUsageFlagBits::eTransferDst,
        vk::Format::eR8G8B8A8Unorm, width, height, vk::ImageLayout::eShaderReadOnlyOptimal);
    if (target_index == kInvalidTargetIndex) {
        return kInvalidTargetIndex;
    }

    auto [staging_buf, staging_memo] = load_into_staging_buffer(
        const_cast<uint8_t*>(pixels), static_cast<uint32_t>(byte_size));
    auto cmd_buf = begin_single_commands();
    auto& target = targets[target_index];
    transit_image_layout(cmd_buf, target.image, vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal);
    copy_buffer_to_image(cmd_buf, staging_buf, target.image, width, height);
    transit_image_layout(cmd_buf, target.image, vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal);
    end_single_commands(std::move(cmd_buf));
    target.layout = vk::ImageLayout::eShaderReadOnlyOptimal;
    return target_index;
}

bool Context::resize_render_target(uint32_t target_index, uint32_t width, uint32_t height) {
    if (target_index >= targets.size()) {
        std::cout << "Render target index out of range: " << target_index << std::endl;
        return false;
    }
    if (width == 0 || height == 0) {
        std::cout << "Render target dimensions must be non-zero" << std::endl;
        return false;
    }

    auto& target = targets[target_index];
    if (target.width == width && target.height == height && target.image != nullptr) {
        return true;
    }

    device.waitIdle();

    vk::ImageAspectFlags aspect_mask = vk::ImageAspectFlagBits::eColor;
    if (target.format == vk::Format::eD32SfloatS8Uint
        || target.format == vk::Format::eD24UnormS8Uint)
    {
        aspect_mask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
    }
    else if (target.format == vk::Format::eD16Unorm || target.format == vk::Format::eD32Sfloat
        || target.format == vk::Format::eX8D24UnormPack32)
    {
        aspect_mask = vk::ImageAspectFlagBits::eDepth;
    }

    const vk::ImageLayout resting_layout = target.descriptor.imageLayout;
    target.view = nullptr;
    target.image = nullptr;
    target.memo = nullptr;
    target.width = width;
    target.height = height;
    std::tie(target.image, target.memo) = create_vk_image(
        width, height, 1, target.samples, target.format, vk::ImageTiling::eOptimal,
        target.usage, vk::MemoryPropertyFlagBits::eDeviceLocal);
    target.view = create_vk_imageview(target.image, target.format, aspect_mask);
    target.layout = vk::ImageLayout::eUndefined;
    target.descriptor = vk::DescriptorImageInfo{*target.sampler, *target.view, resting_layout};

    for (const auto& bind : sampled_attachment_binds) {
        if (!bind.is_depth && bind.attachment_index == target_index) {
            bind_pipeline_render_target(bind.pipeline_name, bind.binding, bind.attachment_index);
        }
    }
    return true;
}

namespace {

size_t render_target_texel_size(vk::Format format) {
    switch (format) {
    case vk::Format::eR32Uint:
    case vk::Format::eR32Sfloat:
        return 4;
    case vk::Format::eR8G8B8A8Unorm:
    case vk::Format::eR8G8B8A8Srgb:
        return 4;
    case vk::Format::eR16G16B16A16Sfloat:
        return 8;
    case vk::Format::eR32G32B32A32Sfloat:
        return 16;
    default:
        return 0;
    }
}

} // namespace

bool Context::read_render_target_pixel(uint32_t target_index, uint32_t x, uint32_t y,
    void* value, size_t value_size)
{
    if (target_index >= targets.size() || value == nullptr) {
        return false;
    }
    auto& target = targets[target_index];
    const size_t texel_size = render_target_texel_size(target.format);
    if (x >= target.width || y >= target.height || texel_size == 0 || value_size != texel_size
        || (target.usage & vk::ImageUsageFlagBits::eTransferSrc) == vk::ImageUsageFlags{})
    {
        return false;
    }

    device.waitIdle();
    auto [readback_buffer, readback_memory] = create_buffer(
        texel_size, vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    auto cmd = begin_single_commands();
    const vk::ImageLayout original_layout = target.layout;
    transit_presentation_image_layout(
        cmd, *target.image, original_layout, vk::ImageLayout::eTransferSrcOptimal,
        vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
        vk::AccessFlagBits2::eTransferRead,
        vk::PipelineStageFlagBits2::eAllCommands, vk::PipelineStageFlagBits2::eTransfer,
        vk::ImageAspectFlagBits::eColor);
    vk::BufferImageCopy copy{};
    copy.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    copy.imageSubresource.layerCount = 1;
    copy.imageOffset = vk::Offset3D{static_cast<int32_t>(x), static_cast<int32_t>(y), 0};
    copy.imageExtent = vk::Extent3D{1, 1, 1};
    cmd.copyImageToBuffer(*target.image, vk::ImageLayout::eTransferSrcOptimal, *readback_buffer, {copy});
    transit_presentation_image_layout(
        cmd, *target.image, vk::ImageLayout::eTransferSrcOptimal, original_layout,
        vk::AccessFlagBits2::eTransferRead,
        vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
        vk::PipelineStageFlagBits2::eTransfer, vk::PipelineStageFlagBits2::eAllCommands,
        vk::ImageAspectFlagBits::eColor);
    end_single_commands(std::move(cmd));

    const void* mapped = readback_memory.mapMemory(0, texel_size);
    std::memcpy(value, mapped, texel_size);
    readback_memory.unmapMemory();
    return true;
}

bool Context::bind_pipeline_render_target(const std::string& pipeline_name, uint32_t binding,
    uint32_t target_index)
{
    const auto pipeline_it = pipelines.find(pipeline_name);
    if (pipeline_it == pipelines.end() || target_index >= targets.size()) {
        return false;
    }

    auto& pipeline = pipeline_it->second;
    const auto sampler_binding = pipeline.sampler_descriptor_counts.find(binding);
    if (sampler_binding == pipeline.sampler_descriptor_counts.end()
        || sampler_binding->second != 1
        || pipeline.descriptor_sets.empty())
    {
        return false;
    }

    const auto& target = targets[target_index];
    if ((target.usage & vk::ImageUsageFlagBits::eSampled) == vk::ImageUsageFlags{}) {
        std::cout << "Render target " << target_index << " was not created with sampled usage"
            << std::endl;
        return false;
    }

    std::vector<vk::DescriptorImageInfo> image_infos(
        pipeline.descriptor_sets.size(), target.descriptor);
    std::vector<vk::WriteDescriptorSet> writes;
    writes.reserve(pipeline.descriptor_sets.size());
    for (size_t i = 0; i < pipeline.descriptor_sets.size(); ++i) {
        vk::WriteDescriptorSet write{};
        write.dstSet = *pipeline.descriptor_sets[i];
        write.dstBinding = binding;
        write.descriptorCount = 1;
        write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        write.pImageInfo = &image_infos[i];
        writes.push_back(write);
    }
    device.updateDescriptorSets(writes, {});

    const auto existing = std::find_if(sampled_attachment_binds.begin(), sampled_attachment_binds.end(),
        [&](const SampledAttachmentBind& bind) {
            return !bind.is_depth && bind.pipeline_name == pipeline_name && bind.binding == binding;
        });
    if (existing != sampled_attachment_binds.end()) {
        existing->attachment_index = target_index;
    }
    else {
        sampled_attachment_binds.push_back(SampledAttachmentBind{
            pipeline_name, binding, target_index, false});
    }
    return true;
}

bool Context::bind_pipeline_texture(const std::string& pipeline_name, uint32_t binding,
    const std::string& texture_name)
{
    const auto pipeline_it = pipelines.find(pipeline_name);
    const auto texture_it = textures.find(texture_name);
    if (pipeline_it == pipelines.end() || texture_it == textures.end() || texture_it->second.vecsize != 1) {
        return false;
    }

    auto& pipeline = pipeline_it->second;
    const auto sampler_binding = pipeline.sampler_descriptor_counts.find(binding);
    if (sampler_binding == pipeline.sampler_descriptor_counts.end()
        || sampler_binding->second != 1 || pipeline.descriptor_sets.empty())
    {
        return false;
    }

    std::vector<vk::DescriptorImageInfo> image_infos(
        pipeline.descriptor_sets.size(), texture_it->second.descriptor);
    std::vector<vk::WriteDescriptorSet> writes;
    writes.reserve(pipeline.descriptor_sets.size());
    for (size_t i = 0; i < pipeline.descriptor_sets.size(); ++i) {
        vk::WriteDescriptorSet write{};
        write.dstSet = *pipeline.descriptor_sets[i];
        write.dstBinding = binding;
        write.descriptorCount = 1;
        write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        write.pImageInfo = &image_infos[i];
        writes.push_back(write);
    }
    device.updateDescriptorSets(writes, {});
    return true;
}

bool Context::bind_pipeline_depth_attachment(const std::string& pipeline_name, uint32_t binding,
    uint32_t attachment_index)
{
    const auto pipeline_it = pipelines.find(pipeline_name);
    if (pipeline_it == pipelines.end() || attachment_index >= depth_attachments.size()) {
        return false;
    }

    auto& pipeline = pipeline_it->second;
    const auto sampler_binding = pipeline.sampler_descriptor_counts.find(binding);
    if (sampler_binding == pipeline.sampler_descriptor_counts.end()
        || sampler_binding->second != 1
        || pipeline.descriptor_sets.empty())
    {
        return false;
    }

    auto& attachment = depth_attachments[attachment_index];
    if (!*attachment.sampler || !*attachment.view) {
        return false;
    }

    attachment.descriptor = vk::DescriptorImageInfo{
        *attachment.sampler, *attachment.view, vk::ImageLayout::eDepthStencilReadOnlyOptimal};

    std::vector<vk::DescriptorImageInfo> image_infos(
        pipeline.descriptor_sets.size(), attachment.descriptor);
    std::vector<vk::WriteDescriptorSet> writes;
    writes.reserve(pipeline.descriptor_sets.size());
    for (size_t i = 0; i < pipeline.descriptor_sets.size(); ++i) {
        vk::WriteDescriptorSet write{};
        write.dstSet = *pipeline.descriptor_sets[i];
        write.dstBinding = binding;
        write.descriptorCount = 1;
        write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        write.pImageInfo = &image_infos[i];
        writes.push_back(write);
    }
    device.updateDescriptorSets(writes, {});

    const auto existing = std::find_if(sampled_attachment_binds.begin(), sampled_attachment_binds.end(),
        [&](const SampledAttachmentBind& bind) {
            return bind.is_depth && bind.pipeline_name == pipeline_name && bind.binding == binding;
        });
    if (existing != sampled_attachment_binds.end()) {
        existing->attachment_index = attachment_index;
    }
    else {
        sampled_attachment_binds.push_back(SampledAttachmentBind{
            pipeline_name, binding, attachment_index, true});
    }
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

bool Context::add_depth_attachment(uint32_t width, uint32_t height,
    vk::Format format, vk::SampleCountFlagBits samples)
{
    const bool match_swapchain = (width == 0 || height == 0);
    const uint32_t attachment_width = width == 0 ? swapchain_extent.width : width;
    const uint32_t attachment_height = height == 0 ? swapchain_extent.height : height;
    if (attachment_width == 0 || attachment_height == 0) {
        std::cout << "Depth attachment dimensions must be non-zero" << std::endl;
        return false;
    }

    DepthAttachment attachment{};
    attachment.width = attachment_width;
    attachment.height = attachment_height;
    attachment.samples = samples;
    attachment.matchSwapchain = match_swapchain;
    // Prefer depth-only formats so one ImageView is valid for both attachment and sampling.
    attachment.format = format == vk::Format::eUndefined ? find_depth_only_format() : format;
    if (attachment.format == vk::Format::eD32SfloatS8Uint
        || attachment.format == vk::Format::eD24UnormS8Uint)
    {
        std::cout << "add_depth_attachment requires a depth-only format "
            "(packed depth+stencil formats need a separate sample view)" << std::endl;
        return false;
    }
    attachment.aspect_mask = vk::ImageAspectFlagBits::eDepth;

    std::tie(attachment.image, attachment.memo) = create_vk_image(
        attachment.width,
        attachment.height,
        1,
        samples,
        attachment.format,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    attachment.view = create_vk_imageview(
        attachment.image,
        attachment.format,
        1,
        attachment.aspect_mask);
    attachment.sampler = create_vk_sampler(
        vk::Filter::eLinear,
        vk::Filter::eLinear,
        vk::SamplerMipmapMode::eNearest,
        vk::SamplerAddressMode::eClampToBorder,
        false,
        true,
        vk::CompareOp::eLessOrEqual);
    attachment.layout = vk::ImageLayout::eUndefined;
    attachment.descriptor = vk::DescriptorImageInfo{
        *attachment.sampler, *attachment.view, vk::ImageLayout::eDepthStencilReadOnlyOptimal};
    depth_attachments.emplace_back(std::move(attachment));
    return true;
}

bool Context::resize_depth_attachment(uint32_t attachment_index, uint32_t width, uint32_t height) {
    if (attachment_index >= depth_attachments.size()) {
        std::cout << "Depth attachment index out of range: " << attachment_index << std::endl;
        return false;
    }
    if (width == 0 || height == 0) {
        std::cout << "Depth attachment dimensions must be non-zero" << std::endl;
        return false;
    }

    auto& attachment = depth_attachments[attachment_index];
    if (attachment.width == width && attachment.height == height && attachment.image != nullptr) {
        return true;
    }

    device.waitIdle();
    attachment.view = nullptr;
    attachment.image = nullptr;
    attachment.memo = nullptr;
    attachment.width = width;
    attachment.height = height;
    attachment.layout = vk::ImageLayout::eUndefined;
    attachment.initialized = false;
    std::tie(attachment.image, attachment.memo) = create_vk_image(
        attachment.width,
        attachment.height,
        1,
        attachment.samples,
        attachment.format,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    attachment.view = create_vk_imageview(
        attachment.image,
        attachment.format,
        1,
        attachment.aspect_mask);
    attachment.descriptor = vk::DescriptorImageInfo{
        *attachment.sampler, *attachment.view, vk::ImageLayout::eDepthStencilReadOnlyOptimal};

    for (const auto& bind : sampled_attachment_binds) {
        if (bind.is_depth && bind.attachment_index == attachment_index) {
            bind_pipeline_depth_attachment(bind.pipeline_name, bind.binding, bind.attachment_index);
        }
    }
    return true;
}

bool Context::set_depth_attachment(uint32_t attachment_index) {
    if (attachment_index >= depth_attachments.size()) {
        std::cout << "Depth attachment index out of range: " << attachment_index << std::endl;
        return false;
    }
    active_depth_attachment_index_ = static_cast<int32_t>(attachment_index);
    return true;
}

void Context::set_default_depth_attachment() {
    active_depth_attachment_index_ = -1;
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
    // Shadow maps: ClampToBorder with far depth (1) so out-of-frustum taps count as lit.
    if (compare_enable && address_mode == vk::SamplerAddressMode::eClampToBorder) {
        sampler_info.borderColor = vk::BorderColor::eFloatOpaqueWhite;
    }
    return vk::raii::Sampler(device, sampler_info);
}

} // namespace vkkk
