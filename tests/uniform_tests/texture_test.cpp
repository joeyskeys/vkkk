#include <catch2/catch_all.hpp>

#include "vk_ins/vkabstraction.h"
#include "vk_ins/vktexture.h"

TEST_CASE("Texture test", "[single-file]") {
    vkkk::VkWrappedInstance ins;
    ins.create_surface();
    ins.create_logical_device();
    ins.create_command_pool();
    vkkk::TextureDeprecated tex(&ins, "tex1", VK_SHADER_STAGE_VERTEX_BIT, 0);
    tex.load_image("D:/repo/floss/vkkk/resource/textures/texture.jpeg");

    REQUIRE(true);
}