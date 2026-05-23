#include <catch2/catch_all.hpp>

#include "vk_ins/vkabstraction.h"
#include "vk_ins/uniform_mgr.h"

TEST_CASE("Uniform Manager test", "[single-file]") {
    vkkk::VkWrappedInstance ins;
    ins.create_surface();
    ins.create_logical_device();
    ins.create_command_pool();

    REQUIRE(true);
}
