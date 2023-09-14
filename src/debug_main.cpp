#include "vk_ins/vkabstraction.h"

using namespace vkkk;

int main() {
    auto ins = VkWrappedInstance();
    ins.init(true);
    ins.setup_resolution(800, 600);

    ins.create_logical_device();
    ins.create_render_target("main", VK_FORMAT_R8G8B8A8_SRGB);
    ins.create_renderpass(VK_FORMAT_R8G8B8A8_SRGB);
    ins.create_command_pool();
    ins.create_color_resource(VK_FORMAT_R8G8B8A8_SRGB);
    ins.create_depth_resource();
    ins.create_framebuffer_from_target("main");
    return 0;
}