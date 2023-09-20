#include "vk_ins/vkabstraction.h"

using namespace vkkk;

int main() {
    auto ins = VkWrappedInstance();
    ins.init(true);
    ins.setup_resolution(800, 600);

    ins.create_logical_device();

    // targets
    ins.create_render_target("color", VK_FORMAT_R8G8B8A8_SRGB);
    auto depth_format = ins.find_depth_format();
    ins.create_render_target("depth", depth_format, VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
    // attachments
    ins.create_attachment(AttachmentType::ATTACH_COLOR, VK_FORMAT_R8G8B8A8_SRGB,
        VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    ins.create_attachment(AttachmentType::ATTACH_DEPTH_STENCIL, depth_format,
        VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    // renderpass
    ins.create_renderpass();

    ins.create_command_pool();
    ins.create_framebuffer_from_target("main");
    return 0;
}