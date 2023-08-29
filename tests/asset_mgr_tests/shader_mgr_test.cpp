#include <catch2/catch_all.hpp>

#include "vk_ins/shader_mgr.h"

TEST_CASE("ShaderMgr test", "shader_mgr") {
    vkkk::ShaderModule m;
    REQUIRE(m.load("../resource/shaders/with_tex.vert", VK_SHADER_STAGE_VERTEX_BIT));
}