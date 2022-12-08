#include <catch2/catch_all.hpp>

#include "vk_ins/vkabstraction.h"
#include "vk_ins/uniform_mgr.h"

TEST_CASE("Manager test", "[single-file]") {
    vkkk::VkWrappedInstance ins;
    ins.create_surface();
    ins.create_logical_device();
    ins.create_command_pool();

    UniformMgr mgr(&ins);
    mgr.add_texture("D:/repo/floss/vkkk/resource/textures/texture.jpeg");
}