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
    init_info.Instance = ctx_->get_vk_instance();
    init_info.PhysicalDevice = ctx_->get_vk_physical_device();
    init_info.Device = ctx_->get_vk_device();
    init_info.QueueFamily = ctx_->get_graphic_queue_family_index();
    init_info.Queue = ctx_->get_vk_queue();
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = descriptor_pool_;
    init_info.Subpass = 0;
    init_info.MinImageCount = ctx_->get_swapchain_count();
    init_info.ImageCount = ctx_->get_swapchain_count();
    init_info.MSAASamples = static_cast<VkSampleCountFlagBits>(ctx_->nsample);
    init_info.UseDynamicRendering = true;
    init_info.ColorAttachmentFormat = ctx_->get_swapchain_format();
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = nullptr;

    if (!ImGui_ImplVulkan_Init(&init_info, VK_NULL_HANDLE)) {
        return false;
    }

    auto cmd = ctx_->begin_single_commands();
    ImGui_ImplVulkan_CreateFontsTexture(*cmd);
    ctx_->end_single_commands(std::move(cmd));
    ImGui_ImplVulkan_DestroyFontUploadObjects();

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