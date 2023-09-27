#include "thirdparty/imgui/imgui.h"
#include "thirdparty/imgui/backends/imgui_impl_glfw.h"
#include "thirdparty/imgui/backends/imgui_impl_vulkan.h"

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>
#include <vulkan/vulkan.h>
#include "vk_ins/vkabstraction.h"

#define WIDTH 800
#define HEIGHT 600

using namespace vkkk;

static void setup_imgui_win(ImGui_ImplVulkanH_Window* win, VkSurfaceKHR surface, int w, int h) {

}

int main() {
    auto ins = VkWrappedInstance();
    ins.setup_resolution(WIDTH, HEIGHT);
    ins.init_glfw();
    ins.create_surface();
    ins.init();

    static ImGui_ImplVulkanH_Window wd;
    setup_imgui_win(&wd, ins.get_surface(), WIDTH, HEIGHT);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(ins.get_window(), true);
    ImGui_ImplVulkan_InitInfo init_info{
        
    };
}