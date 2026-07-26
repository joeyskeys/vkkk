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

bool Context::bind(vk::CommandBuffer cmd, const std::string& pipeline_name, uint32_t frame_idx) const
{
    const auto pipeline_it = pipelines.find(pipeline_name);
    if (pipeline_it == pipelines.end()) {
        return false;
    }
    const auto& pipeline = pipeline_it->second;
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline.vk_pipeline);
    if (!pipeline.descriptor_sets.empty()) {
        const auto idx = std::min(frame_idx, static_cast<uint32_t>(pipeline.descriptor_sets.size() - 1));
        const vk::DescriptorSet desc_set = *pipeline.descriptor_sets[idx];
        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, *pipeline.vk_pipeline_layout, 0, {desc_set}, {});
    }
    return true;
}

bool Context::draw(vk::CommandBuffer cmd, const std::string& pipeline_name, const std::string& mesh_name,
    uint32_t instance_count, uint32_t instance_offset) const
{
    const auto pipeline_it = pipelines.find(pipeline_name);
    if (pipeline_it == pipelines.end()) {
        return false;
    }
    return draw_mesh_instanced(
        cmd, mesh_name, *pipeline_it->second.vk_pipeline_layout, instance_count, instance_offset, nullptr);
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

namespace {

vk::AttachmentLoadOp to_vk_load_op(PassLoadOp op) {
    switch (op) {
    case PassLoadOp::Load: return vk::AttachmentLoadOp::eLoad;
    case PassLoadOp::DontCare: return vk::AttachmentLoadOp::eDontCare;
    case PassLoadOp::Clear:
    default: return vk::AttachmentLoadOp::eClear;
    }
}

vk::AttachmentStoreOp to_vk_store_op(PassStoreOp op) {
    switch (op) {
    case PassStoreOp::DontCare: return vk::AttachmentStoreOp::eDontCare;
    case PassStoreOp::Store:
    default: return vk::AttachmentStoreOp::eStore;
    }
}

} // namespace

void Context::begin_cmds(uint32_t image_index) {
    auto& cmd = command_buffers[image_index];
    cmd.reset({});
    cmd.begin({});
}

void Context::end_cmds(uint32_t image_index) {
    command_buffers[image_index].end();
}

void Context::begin_pass(vk::raii::CommandBuffer& cmd, uint32_t image_index, const PassDesc& pass) {
    if (image_index >= swapchain_images.size()) {
        throw std::runtime_error("begin_pass: image_index out of range");
    }

    std::vector<vk::RenderingAttachmentInfo> color_infos;
    color_infos.reserve(pass.colors.size());
    vk::Extent2D render_extent{0, 0};

    for (const auto& color : pass.colors) {
        vk::ImageView view = nullptr;
        vk::Image image = nullptr;
        vk::Extent2D extent{};
        vk::ImageLayout old_layout = vk::ImageLayout::eUndefined;

        if (color.target_index == kSwapchainTarget) {
            view = *swapchain_image_views[image_index];
            image = swapchain_images[image_index];
            extent = swapchain_extent;
            old_layout = vk::ImageLayout::eUndefined;
            transit_presentation_image_layout(
                cmd, image, old_layout, vk::ImageLayout::eColorAttachmentOptimal,
                {}, vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::ImageAspectFlagBits::eColor);
        }
        else {
            if (color.target_index < 0
                || static_cast<size_t>(color.target_index) >= targets.size())
            {
                throw std::runtime_error("begin_pass: color target_index out of range");
            }
            auto& target = targets[static_cast<size_t>(color.target_index)];
            view = *target.view;
            image = *target.image;
            extent = vk::Extent2D{target.width, target.height};
            old_layout = target.layout;
            transit_presentation_image_layout(
                cmd, image, old_layout, vk::ImageLayout::eColorAttachmentOptimal,
                vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::PipelineStageFlagBits2::eAllCommands,
                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::ImageAspectFlagBits::eColor);
            target.layout = vk::ImageLayout::eColorAttachmentOptimal;
        }

        if (render_extent.width == 0) {
            render_extent = extent;
        }
        else if (extent.width != render_extent.width || extent.height != render_extent.height) {
            throw std::runtime_error("begin_pass: MRT color targets must share the same extent");
        }

        vk::RenderingAttachmentInfo info{};
        info.imageView = view;
        info.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        info.loadOp = to_vk_load_op(color.load);
        info.storeOp = to_vk_store_op(color.store);
        info.clearValue = vk::ClearColorValue(color.clear);
        color_infos.push_back(info);
    }

    vk::RenderingAttachmentInfo depth_info{};
    const vk::RenderingAttachmentInfo* depth_ptr = nullptr;
    const bool use_depth = pass.depth_index != kNoDepth;

    if (use_depth) {
        vk::Image depth_image_handle = nullptr;
        vk::ImageView depth_view_handle = nullptr;
        vk::ImageAspectFlags depth_aspect = vk::ImageAspectFlagBits::eDepth;
        vk::Extent2D depth_extent{};
        bool* depth_initialized = nullptr;
        vk::ImageLayout* depth_layout = nullptr;
        DepthAttachment* custom_depth = nullptr;

        if (pass.depth_index == kDefaultDepth) {
            if (active_depth_attachment_index_ >= 0
                && static_cast<size_t>(active_depth_attachment_index_) < depth_attachments.size())
            {
                custom_depth =
                    &depth_attachments[static_cast<size_t>(active_depth_attachment_index_)];
            }
        }
        else if (pass.depth_index >= 0
            && static_cast<size_t>(pass.depth_index) < depth_attachments.size())
        {
            custom_depth = &depth_attachments[static_cast<size_t>(pass.depth_index)];
        }
        else if (pass.depth_index != kDefaultDepth) {
            throw std::runtime_error("begin_pass: depth_index out of range");
        }

        if (custom_depth != nullptr) {
            depth_image_handle = *custom_depth->image;
            depth_view_handle = *custom_depth->view;
            depth_aspect = custom_depth->aspect_mask;
            depth_extent = vk::Extent2D{custom_depth->width, custom_depth->height};
            depth_initialized = &custom_depth->initialized;
            depth_layout = &custom_depth->layout;
        }
        else {
            depth_image_handle = *depth_image;
            depth_view_handle = *depth_view;
            const vk::Format default_depth_format = find_depth_format();
            if (default_depth_format == vk::Format::eD32SfloatS8Uint
                || default_depth_format == vk::Format::eD24UnormS8Uint)
            {
                depth_aspect |= vk::ImageAspectFlagBits::eStencil;
            }
            depth_extent = swapchain_extent;
            depth_initialized = &depth_image_initialized;
        }

        if (render_extent.width == 0) {
            render_extent = depth_extent;
        }
        else if (depth_extent.width != render_extent.width
            || depth_extent.height != render_extent.height)
        {
            throw std::runtime_error("begin_pass: depth extent does not match color targets");
        }

        const vk::ImageLayout old_depth_layout = (depth_layout != nullptr && *depth_initialized)
            ? *depth_layout
            : (*depth_initialized ? vk::ImageLayout::eDepthStencilAttachmentOptimal
                                  : vk::ImageLayout::eUndefined);
        const bool from_sampled = old_depth_layout == vk::ImageLayout::eDepthStencilReadOnlyOptimal;
        const bool from_undefined = old_depth_layout == vk::ImageLayout::eUndefined;

        if (from_undefined || from_sampled || !*depth_initialized) {
            transit_presentation_image_layout(
                cmd, depth_image_handle,
                from_undefined || !*depth_initialized
                    ? vk::ImageLayout::eUndefined : old_depth_layout,
                vk::ImageLayout::eDepthStencilAttachmentOptimal,
                from_sampled ? vk::AccessFlagBits2::eShaderRead : vk::AccessFlags2{},
                vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                from_sampled ? vk::PipelineStageFlagBits2::eFragmentShader
                             : vk::PipelineStageFlagBits2::eTopOfPipe,
                vk::PipelineStageFlagBits2::eEarlyFragmentTests
                    | vk::PipelineStageFlagBits2::eLateFragmentTests,
                depth_aspect);
        }
        else {
            transit_presentation_image_layout(
                cmd, depth_image_handle,
                vk::ImageLayout::eDepthStencilAttachmentOptimal,
                vk::ImageLayout::eDepthStencilAttachmentOptimal,
                vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                vk::PipelineStageFlagBits2::eLateFragmentTests,
                vk::PipelineStageFlagBits2::eEarlyFragmentTests
                    | vk::PipelineStageFlagBits2::eLateFragmentTests,
                depth_aspect);
        }
        *depth_initialized = true;
        if (depth_layout != nullptr) {
            *depth_layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
        }

        depth_info.imageView = depth_view_handle;
        depth_info.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
        depth_info.loadOp = to_vk_load_op(pass.depth_load);
        depth_info.storeOp = to_vk_store_op(pass.depth_store);
        depth_info.clearValue = vk::ClearDepthStencilValue(pass.depth_clear, 0);
        depth_ptr = &depth_info;
    }

    if (render_extent.width == 0 || render_extent.height == 0) {
        throw std::runtime_error("begin_pass: no color or depth target to determine extent");
    }

    vk::RenderingInfo rendering_info{};
    rendering_info.renderArea.offset = vk::Offset2D{0, 0};
    rendering_info.renderArea.extent = render_extent;
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = static_cast<uint32_t>(color_infos.size());
    rendering_info.pColorAttachments = color_infos.empty() ? nullptr : color_infos.data();
    rendering_info.pDepthAttachment = depth_ptr;

    cmd.beginRendering(rendering_info);
    cmd.setViewport(0, vk::Viewport{
        0.f, 0.f,
        static_cast<float>(render_extent.width),
        static_cast<float>(render_extent.height),
        0.f, 1.f
    });
    cmd.setScissor(0, vk::Rect2D{{0, 0}, render_extent});
}

void Context::end_pass(vk::raii::CommandBuffer& cmd, uint32_t image_index, const PassDesc& pass) {
    if (image_index >= swapchain_images.size()) {
        throw std::runtime_error("end_pass: image_index out of range");
    }

    cmd.endRendering();

    for (const auto& color : pass.colors) {
        if (color.target_index == kSwapchainTarget) {
            if (pass.present) {
                transit_presentation_image_layout(
                    cmd, swapchain_images[image_index],
                    vk::ImageLayout::eColorAttachmentOptimal,
                    vk::ImageLayout::ePresentSrcKHR,
                    vk::AccessFlagBits2::eColorAttachmentWrite, {},
                    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                    vk::PipelineStageFlagBits2::eBottomOfPipe,
                    vk::ImageAspectFlagBits::eColor);
            }
            continue;
        }
        if (color.target_index < 0
            || static_cast<size_t>(color.target_index) >= targets.size())
        {
            throw std::runtime_error("end_pass: color target_index out of range");
        }
        auto& target = targets[static_cast<size_t>(color.target_index)];
        // Restore to the layout used for sampling/general access after the write.
        const vk::ImageLayout restore_layout = target.descriptor.imageLayout;
        transit_presentation_image_layout(
            cmd, *target.image,
            vk::ImageLayout::eColorAttachmentOptimal, restore_layout,
            vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::PipelineStageFlagBits2::eAllCommands,
            vk::ImageAspectFlagBits::eColor);
        target.layout = restore_layout;
    }

    // Depth-only custom attachments (e.g. shadow maps) become sampleable after the pass.
    if (pass.colors.empty() && pass.depth_index >= 0
        && static_cast<size_t>(pass.depth_index) < depth_attachments.size())
    {
        auto& attachment = depth_attachments[static_cast<size_t>(pass.depth_index)];
        transit_presentation_image_layout(
            cmd, *attachment.image,
            vk::ImageLayout::eDepthStencilAttachmentOptimal,
            vk::ImageLayout::eDepthStencilReadOnlyOptimal,
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::AccessFlagBits2::eShaderRead,
            vk::PipelineStageFlagBits2::eLateFragmentTests,
            vk::PipelineStageFlagBits2::eFragmentShader,
            attachment.aspect_mask);
        attachment.layout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
        attachment.initialized = true;
    }
}

bool Context::record_depth_pass(vk::raii::CommandBuffer& cmd, uint32_t attachment_index,
    const std::function<void(vk::raii::CommandBuffer&)>& emit_func)
{
    if (attachment_index >= depth_attachments.size() || !emit_func) {
        return false;
    }

    PassDesc pass{};
    pass.colors.clear();
    pass.depth_index = static_cast<int32_t>(attachment_index);
    pass.present = false;
    begin_pass(cmd, 0, pass);
    emit_func(cmd);
    end_pass(cmd, 0, pass);
    return true;
}

void Context::record_cmds(uint32_t image_index,
    const std::function<void(vk::raii::CommandBuffer&, uint32_t)>& emit_func,
    const std::function<void(vk::raii::CommandBuffer&, uint32_t)>& pre_render_func)
{
    begin_cmds(image_index);
    auto& cmd_buf = command_buffers[image_index];
    if (pre_render_func) {
        pre_render_func(cmd_buf, image_index);
    }

    PassDesc pass{};
    if (active_render_target_index_ >= 0
        && static_cast<size_t>(active_render_target_index_) < targets.size())
    {
        pass.colors = { ColorTargetRef{
            .target_index = active_render_target_index_,
        } };
        pass.present = false;
    }
    if (active_depth_attachment_index_ >= 0
        && static_cast<size_t>(active_depth_attachment_index_) < depth_attachments.size())
    {
        pass.depth_index = active_depth_attachment_index_;
    }

    begin_pass(cmd_buf, image_index, pass);
    emit_func(cmd_buf, image_index);
    end_pass(cmd_buf, image_index, pass);

    // draw_frame always presents; if this pass did not touch the swapchain, still
    // move it to PresentSrc so the queue present is valid.
    if (!pass.present) {
        transit_presentation_image_layout(
            cmd_buf, swapchain_images[image_index],
            vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR,
            {}, {},
            vk::PipelineStageFlagBits2::eTopOfPipe,
            vk::PipelineStageFlagBits2::eBottomOfPipe,
            vk::ImageAspectFlagBits::eColor);
    }
    end_cmds(image_index);
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
