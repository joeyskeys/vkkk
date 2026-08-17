#include <algorithm>
#include <cstring>
#include <limits>

#include "vk_ins/context.hpp"

namespace vkkk
{

namespace
{

constexpr uint32_t kEmptyABufferHead = ~0u;
constexpr uint32_t kABufferHeaderWords = 4;

bool valid_abuffer_size(vk::Extent2D extent, uint32_t nodes_per_pixel, uint32_t& capacity) {
    if (extent.width == 0 || extent.height == 0 || nodes_per_pixel == 0) {
        return false;
    }
    const uint64_t pixel_count = static_cast<uint64_t>(extent.width) * extent.height;
    if (pixel_count > std::numeric_limits<uint32_t>::max() / nodes_per_pixel) {
        return false;
    }
    const uint64_t count = pixel_count * nodes_per_pixel;
    if (count == 0 || count > std::numeric_limits<uint32_t>::max()) {
        return false;
    }
    capacity = static_cast<uint32_t>(count);
    return true;
}

} // namespace

bool Context::create_abuffer(const std::string& name, vk::Extent2D extent, uint32_t nodes_per_pixel) {
    if (name.empty() || abuffers.contains(name)) {
        return false;
    }

    uint32_t node_capacity = 0;
    if (!valid_abuffer_size(extent, nodes_per_pixel, node_capacity)) {
        return false;
    }

    const vk::DeviceSize head_bytes = (static_cast<vk::DeviceSize>(kABufferHeaderWords)
        + static_cast<vk::DeviceSize>(extent.width) * extent.height) * sizeof(uint32_t);
    const vk::DeviceSize node_bytes = static_cast<vk::DeviceSize>(kABufferHeaderWords) * sizeof(uint32_t)
        + static_cast<vk::DeviceSize>(node_capacity) * sizeof(ABufferNode);
    ABuffer buffer{};
    buffer.width = extent.width;
    buffer.height = extent.height;
    buffer.nodes_per_pixel = nodes_per_pixel;
    buffer.node_capacity = node_capacity;
    buffer.head_bytes = head_bytes;
    buffer.node_bytes = node_bytes;
    buffer.head_buffers.reserve(swapchain_images.size());
    buffer.head_memos.reserve(swapchain_images.size());
    buffer.node_buffers.reserve(swapchain_images.size());
    buffer.node_memos.reserve(swapchain_images.size());
    for (size_t index = 0; index < swapchain_images.size(); ++index) {
        auto [head, head_memo] = create_buffer(head_bytes, vk::BufferUsageFlagBits::eStorageBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        auto [nodes, node_memo] = create_buffer(node_bytes, vk::BufferUsageFlagBits::eStorageBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        buffer.head_buffers.push_back(std::move(head));
        buffer.head_memos.push_back(std::move(head_memo));
        buffer.node_buffers.push_back(std::move(nodes));
        buffer.node_memos.push_back(std::move(node_memo));
    }
    abuffers.emplace(name, std::move(buffer));
    for (uint32_t index = 0; index < get_swapchain_count(); ++index) {
        clear_abuffer(name, index);
    }
    return true;
}

bool Context::resize_abuffer(const std::string& name, vk::Extent2D extent, uint32_t nodes_per_pixel) {
    const auto found = abuffers.find(name);
    if (found == abuffers.end()) {
        return create_abuffer(name, extent, nodes_per_pixel);
    }
    if (found->second.width == extent.width && found->second.height == extent.height
        && found->second.nodes_per_pixel == nodes_per_pixel)
    {
        return true;
    }
    abuffers.erase(found);
    return create_abuffer(name, extent, nodes_per_pixel);
}

bool Context::clear_abuffer(const std::string& name, uint32_t frame_idx) {
    const auto found = abuffers.find(name);
    if (found == abuffers.end() || frame_idx >= found->second.head_memos.size()) {
        return false;
    }

    const auto& buffer = found->second;
    auto* heads = static_cast<uint32_t*>(buffer.head_memos[frame_idx].mapMemory(0, buffer.head_bytes));
    heads[0] = buffer.width;
    heads[1] = buffer.height;
    heads[2] = 0;
    heads[3] = 0;
    std::fill_n(heads + kABufferHeaderWords,
        static_cast<size_t>(buffer.width) * buffer.height, kEmptyABufferHead);
    buffer.head_memos[frame_idx].unmapMemory();

    auto* node_header = static_cast<uint32_t*>(buffer.node_memos[frame_idx].mapMemory(0, buffer.node_bytes));
    node_header[0] = 0;
    node_header[1] = 0;
    node_header[2] = buffer.node_capacity;
    node_header[3] = 0;
    buffer.node_memos[frame_idx].unmapMemory();
    return true;
}

bool Context::bind_pipeline_abuffer(const std::string& pipeline_name, const std::string& abuffer_name,
    uint32_t head_binding, uint32_t node_binding)
{
    const auto pipeline_it = pipelines.find(pipeline_name);
    const auto buffer_it = abuffers.find(abuffer_name);
    if (pipeline_it == pipelines.end() || buffer_it == abuffers.end()) {
        return false;
    }

    auto& pipeline = pipeline_it->second;
    const auto& buffer = buffer_it->second;
    if (pipeline.descriptor_sets.size() != buffer.head_buffers.size()) {
        return false;
    }

    std::vector<vk::DescriptorBufferInfo> infos;
    std::vector<vk::WriteDescriptorSet> writes;
    infos.reserve(pipeline.descriptor_sets.size() * 2);
    writes.reserve(pipeline.descriptor_sets.size() * 2);
    for (size_t index = 0; index < pipeline.descriptor_sets.size(); ++index) {
        infos.push_back(vk::DescriptorBufferInfo{*buffer.head_buffers[index], 0, buffer.head_bytes});
        vk::WriteDescriptorSet head_write{};
        head_write.dstSet = *pipeline.descriptor_sets[index];
        head_write.dstBinding = head_binding;
        head_write.descriptorCount = 1;
        head_write.descriptorType = vk::DescriptorType::eStorageBuffer;
        head_write.pBufferInfo = &infos.back();
        writes.push_back(head_write);

        infos.push_back(vk::DescriptorBufferInfo{*buffer.node_buffers[index], 0, buffer.node_bytes});
        vk::WriteDescriptorSet node_write{};
        node_write.dstSet = *pipeline.descriptor_sets[index];
        node_write.dstBinding = node_binding;
        node_write.descriptorCount = 1;
        node_write.descriptorType = vk::DescriptorType::eStorageBuffer;
        node_write.pBufferInfo = &infos.back();
        writes.push_back(node_write);
    }
    device.updateDescriptorSets(writes, {});
    return true;
}

void Context::barrier_abuffer_for_host(vk::CommandBuffer cmd) const {
    vk::MemoryBarrier2 barrier{};
    barrier.srcStageMask = vk::PipelineStageFlagBits2::eFragmentShader;
    barrier.srcAccessMask = vk::AccessFlagBits2::eShaderStorageWrite;
    barrier.dstStageMask = vk::PipelineStageFlagBits2::eHost;
    barrier.dstAccessMask = vk::AccessFlagBits2::eHostRead;
    vk::DependencyInfo dependency{};
    dependency.memoryBarrierCount = 1;
    dependency.pMemoryBarriers = &barrier;
    cmd.pipelineBarrier2(dependency);
}

bool Context::read_abuffer_pixel(const std::string& name, uint32_t frame_idx, uint32_t x, uint32_t y,
    std::vector<uint32_t>& vertex_ids, bool& overflow) const
{
    vertex_ids.clear();
    overflow = false;
    const auto found = abuffers.find(name);
    if (found == abuffers.end() || frame_idx >= found->second.head_memos.size()
        || x >= found->second.width || y >= found->second.height)
    {
        return false;
    }

    device.waitIdle();
    const auto& buffer = found->second;
    const auto* heads = static_cast<const uint32_t*>(
        buffer.head_memos[frame_idx].mapMemory(0, buffer.head_bytes));
    const uint32_t head = heads[kABufferHeaderWords + y * buffer.width + x];
    buffer.head_memos[frame_idx].unmapMemory();

    const auto* words = static_cast<const uint32_t*>(
        buffer.node_memos[frame_idx].mapMemory(0, buffer.node_bytes));
    overflow = words[1] != 0;
    const auto* nodes = reinterpret_cast<const ABufferNode*>(words + kABufferHeaderWords);
    uint32_t node_index = head;
    for (uint32_t traversed = 0; node_index != kEmptyABufferHead
        && traversed < buffer.node_capacity; ++traversed)
    {
        if (node_index >= buffer.node_capacity) {
            buffer.node_memos[frame_idx].unmapMemory();
            return false;
        }
        vertex_ids.push_back(nodes[node_index].vertex_id);
        node_index = nodes[node_index].next;
    }
    buffer.node_memos[frame_idx].unmapMemory();
    return true;
}

} // namespace vkkk
