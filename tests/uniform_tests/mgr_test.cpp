#include <catch2/catch_all.hpp>

#include "vk_ins/vkabstraction.h"
#include "vk_ins/uniform_mgr.h"

TEST_CASE("Uniform Manager test", "[single-file]") {
    vkkk::VkWrappedInstance ins;
    ins.create_surface();
    ins.create_logical_device();
    ins.create_command_pool();

    vkkk::UniformMgr mgr(&ins);
    mgr.add_texture("test_tex", VK_SHADER_STAGE_VERTEX_BIT, 0, "D:/repo/floss/vkkk/resource/textures/texture.jpeg");

    REQUIRE(true);
}
