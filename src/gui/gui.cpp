#include <array>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include "gui/gui.h"
#include "vk_ins/vkabstraction.h"

namespace vkkk
{

bool ImGuiHud::init(VkWrappedInstance* ins) {
    if (initialized_) {
        return true;
    }

    if (ins == nullptr || ins->get_window() == nullptr) {
        return false;
    }

    ins_ = ins;

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
    if (vkCreateDescriptorPool(ins_->get_device(), &pool_info, nullptr, &descriptor_pool_) != VK_SUCCESS) {
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(ins_->get_window(), false);

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = ins_->get_instance();
    init_info.PhysicalDevice = ins_->get_physical_device();
    init_info.Device = ins_->get_device();
    init_info.QueueFamily = ins_->get_graphic_queue_family_index();
    init_info.Queue = ins_->get_graphic_queue();
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = descriptor_pool_;
    init_info.Subpass = 0;
    init_info.MinImageCount = ins_->get_swapchain_cnt();
    init_info.ImageCount = ins_->get_swapchain_cnt();
    init_info.MSAASamples = ins_->nsample;
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = nullptr;

    if (!ImGui_ImplVulkan_Init(&init_info, ins_->get_renderpass())) {
        return false;
    }

    VkCommandBuffer cmd = ins_->begin_single_time_commands();
    ImGui_ImplVulkan_CreateFontsTexture(cmd);
    ins_->end_single_time_commands(cmd);
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

    vkDeviceWaitIdle(ins_->get_device());
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (descriptor_pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(ins_->get_device(), descriptor_pool_, nullptr);
        descriptor_pool_ = VK_NULL_HANDLE;
    }

    initialized_ = false;
    ins_ = nullptr;
}

} // namespace vkkk