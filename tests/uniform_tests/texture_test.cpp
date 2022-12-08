#include <catch2/catch_all.hpp>

#include "vk_ins/vkabstraction.h"
#include "vk_ins/vktexture.h"

TEST_CASE("Texture test", "[single-file]") {
    vkkk::VkWrappedInstance ins;
    ins.create_surface();
    ins.create_logical_device();
    ins.create_command_pool();
    vkkk::Texture tex(&ins);
    tex.load_image("D:/repo/floss/vkkk/resource/textures/texture.jpeg");

    REQUIRE(true);
}