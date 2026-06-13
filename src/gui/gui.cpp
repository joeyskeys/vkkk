#include <array>
#include <utility>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include "gui/gui.h"
#include "vk_ins/context.hpp"

namespace vkkk
{

bool ImGuiHud::init(Context* ctx) {
    if (initialized_) {
        return true;
    }

    if (ctx == nullptr || ctx->get_window() == nullptr) {
        return false;
    }

    ctx_ = ctx;

    const std::array<VkDescriptorPoolSize, 11> pool_sizes{{
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    }};

    VkDescriptorPoolCreateInfo pool_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1000 * static_cast<uint32_t>(pool_sizes.size()),
        .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
        .pPoolSizes = pool_sizes.data()
    };
    if (vkCreateDescriptorPool(ctx_->get_vk_device(), &pool_info, nullptr, &descriptor_pool_) != VK_SUCCESS) {
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(ctx_->get_window(), false);

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.ApiVersion = VK_API_VERSION_1_3;
    init_info.Instance = ctx_->get_vk_instance();
    init_info.PhysicalDevice = ctx_->get_vk_physical_device();
    init_info.Device = ctx_->get_vk_device();
    init_info.QueueFamily = ctx_->get_graphic_queue_family_index();
    init_info.Queue = ctx_->get_vk_queue();
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = descriptor_pool_;
    init_info.DescriptorPoolSize = 0;
    init_info.MinImageCount = ctx_->get_swapchain_count();
    init_info.ImageCount = ctx_->get_swapchain_count();
    init_info.PipelineInfoMain.RenderPass = VK_NULL_HANDLE;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = static_cast<VkSampleCountFlagBits>(ctx_->nsample);
    init_info.UseDynamicRendering = true;
    VkPipelineRenderingCreateInfoKHR rendering_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .pNext = nullptr,
        .viewMask = 0,
        .colorAttachmentCount = 1
    };
    const VkFormat color_format = ctx_->get_swapchain_format();
    rendering_info.pColorAttachmentFormats = &color_format;
    const VkFormat depth_format = ctx_->get_depth_format();
    rendering_info.depthAttachmentFormat = depth_format;
    const bool has_stencil = depth_format == VK_FORMAT_D32_SFLOAT_S8_UINT
        || depth_format == VK_FORMAT_D24_UNORM_S8_UINT;
    rendering_info.stencilAttachmentFormat = has_stencil ? depth_format : VK_FORMAT_UNDEFINED;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo = rendering_info;
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = nullptr;

    if (!ImGui_ImplVulkan_Init(&init_info)) {
        return false;
    }

    initialized_ = true;
    return true;
}

void ImGuiHud::begin_frame() {
    if (!initialized_) {
        return;
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiHud::build_default_hud(float fps, uint32_t drawable_count) {
    if (!initialized_) {
        return;
    }

    ImGui::Begin("HUD");
    ImGui::Text("Renderer: Vulkan + ImGui");
    ImGui::Separator();
    ImGui::Text("FPS: %.2f", fps);
    ImGui::Text("Drawables: %u", drawable_count);
    ImGui::End();
}

void ImGuiHud::render(VkCommandBuffer cmd) {
    if (!initialized_) {
        return;
    }

    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    ImGui_ImplVulkan_RenderDrawData(draw_data, cmd);
}

void ImGuiHud::shutdown() {
    if (!initialized_) {
        return;
    }

    ctx_->wait_idle();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (descriptor_pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(ctx_->get_vk_device(), descriptor_pool_, nullptr);
        descriptor_pool_ = VK_NULL_HANDLE;
    }

    initialized_ = false;
    ctx_ = nullptr;
}

} // namespace vkkk