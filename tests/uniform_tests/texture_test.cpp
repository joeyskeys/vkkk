#include <catch2/catch_all.hpp>

#include "vk_ins/vkabstraction.h"

TEST_CASE("Texture test", "[single-file]") {
    vkkk::VkWrappedInstance ins;
    ins.create_surface();
    ins.create_logical_device();
    ins.create_command_pool();

    REQUIRE(true);
}