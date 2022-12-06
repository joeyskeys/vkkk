#include <catch2/catch_all.hpp>

#include "vk_ins/vkabstraction.h"
#include "vk_ins/vktexture.h"

TEST_CASE("Texture test", "[single-file]") {
    vkkk::VkWrappedInstance ins;
    ins.create_surface();
    ins.create_logical_device();
    vkkk::Texture tex(&ins);
    tex.load_image("../resource/textures/texture.jpeg");

    REQUIRE(true);
}