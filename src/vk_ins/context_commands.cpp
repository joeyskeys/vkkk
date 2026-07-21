#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>

#include "vk_ins/context.hpp"

namespace vkkk
{

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

bool Context::record_depth_pass(vk::raii::CommandBuffer& cmd, uint32_t attachment_index,
    const std::function<void(vk::raii::CommandBuffer&)>& emit_func)
{
    if (attachment_index >= depth_attachments.size() || !emit_func) {
        return false;
    }

    auto& attachment = depth_attachments[attachment_index];
    const vk::Image depth_image_handle = *attachment.image;
    const vk::ImageAspectFlags aspect_mask = attachment.aspect_mask;
    const vk::ImageLayout old_layout = attachment.initialized
        ? attachment.layout : vk::ImageLayout::eUndefined;

    transit_presentation_image_layout(
        cmd,
        depth_image_handle,
        old_layout,
        vk::ImageLayout::eDepthStencilAttachmentOptimal,
        old_layout == vk::ImageLayout::eDepthStencilReadOnlyOptimal
            ? vk::AccessFlagBits2::eShaderRead : vk::AccessFlags2{},
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        old_layout == vk::ImageLayout::eDepthStencilReadOnlyOptimal
            ? vk::PipelineStageFlagBits2::eFragmentShader
            : vk::PipelineStageFlagBits2::eTopOfPipe,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests
            | vk::PipelineStageFlagBits2::eLateFragmentTests,
        aspect_mask);

    vk::RenderingAttachmentInfo depth_attachment_info{};
    depth_attachment_info.imageView = *attachment.view;
    depth_attachment_info.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    depth_attachment_info.loadOp = vk::AttachmentLoadOp::eClear;
    depth_attachment_info.storeOp = vk::AttachmentStoreOp::eStore;
    depth_attachment_info.clearValue = vk::ClearDepthStencilValue(1.f, 0);

    const vk::Extent2D render_extent{attachment.width, attachment.height};
    vk::RenderingInfo rendering_info{};
    rendering_info.renderArea.offset = vk::Offset2D{0, 0};
    rendering_info.renderArea.extent = render_extent;
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = 0;
    rendering_info.pDepthAttachment = &depth_attachment_info;

    cmd.beginRendering(rendering_info);
    cmd.setViewport(0, vk::Viewport{
        0.f, 0.f,
        static_cast<float>(render_extent.width),
        static_cast<float>(render_extent.height),
        0.f, 1.f
    });
    cmd.setScissor(0, vk::Rect2D{{0, 0}, render_extent});
    emit_func(cmd);
    cmd.endRendering();

    transit_presentation_image_layout(
        cmd,
        depth_image_handle,
        vk::ImageLayout::eDepthStencilAttachmentOptimal,
        vk::ImageLayout::eDepthStencilReadOnlyOptimal,
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        vk::AccessFlagBits2::eShaderRead,
        vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::PipelineStageFlagBits2::eFragmentShader,
        aspect_mask);
    attachment.layout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
    attachment.initialized = true;
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
    DepthAttachment* active_depth_attachment = nullptr;
    if (active_depth_attachment_index_ >= 0
        && static_cast<size_t>(active_depth_attachment_index_) < depth_attachments.size())
    {
        active_depth_attachment =
            &depth_attachments[static_cast<size_t>(active_depth_attachment_index_)];
    }
    const vk::Image active_depth_image = active_depth_attachment != nullptr
        ? static_cast<vk::Image>(*active_depth_attachment->image)
        : static_cast<vk::Image>(*depth_image);
    const vk::ImageView active_depth_view = active_depth_attachment != nullptr
        ? static_cast<vk::ImageView>(*active_depth_attachment->view)
        : static_cast<vk::ImageView>(*depth_view);
    vk::ImageAspectFlags default_depth_aspect = vk::ImageAspectFlagBits::eDepth;
    const vk::Format default_depth_format = find_depth_format();
    if (default_depth_format == vk::Format::eD32SfloatS8Uint
        || default_depth_format == vk::Format::eD24UnormS8Uint)
    {
        default_depth_aspect |= vk::ImageAspectFlagBits::eStencil;
    }
    const vk::ImageAspectFlags active_depth_aspect =
        active_depth_attachment != nullptr ? active_depth_attachment->aspect_mask : default_depth_aspect;
    bool& active_depth_initialized = active_depth_attachment != nullptr
        ? active_depth_attachment->initialized
        : depth_image_initialized;

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

    if (!active_depth_initialized) {
        transit_presentation_image_layout(
            cmd_buf,
            active_depth_image,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eDepthStencilAttachmentOptimal,
            {},
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::PipelineStageFlagBits2::eTopOfPipe,
            vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
            active_depth_aspect);
        active_depth_initialized = true;
    }
    else {
        // Keep the selected depth image in attachment layout across frames and insert
        // an explicit dependency between consecutive depth write passes.
        transit_presentation_image_layout(
            cmd_buf,
            active_depth_image,
            vk::ImageLayout::eDepthStencilAttachmentOptimal,
            vk::ImageLayout::eDepthStencilAttachmentOptimal,
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::PipelineStageFlagBits2::eLateFragmentTests,
            vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
            active_depth_aspect);
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
    depth_attachment_info.imageView = active_depth_view;
    depth_attachment_info.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    depth_attachment_info.loadOp = vk::AttachmentLoadOp::eClear;
    depth_attachment_info.storeOp = vk::AttachmentStoreOp::eStore;
    depth_attachment_info.clearValue = depth_clear_value;
    vk::RenderingInfo rendering_info{};
    rendering_info.renderArea.offset = vk::Offset2D{0, 0};
    const vk::Extent2D render_extent = active_target == nullptr
        ? swapchain_extent
        : vk::Extent2D{active_target->width, active_target->height};
    if (active_depth_attachment != nullptr
        && (active_depth_attachment->width != render_extent.width
            || active_depth_attachment->height != render_extent.height))
    {
        throw std::runtime_error("selected depth attachment dimensions do not match render target");
    }
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

} // namespace vkkk
