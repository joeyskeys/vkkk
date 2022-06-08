#include <vector>

#define CARCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>
#include <spirv_cross/spirv.hpp>
#include <spirv_cross/spirv_glsl.hpp>

#include "utils/io.h"

TEST_CASE("Resource test", "[single-file]") {
    std::vector<uint32_t> spirv = load_spirv_file("../resource/shaders/mvp_default_vert.spv");
    if (spirv.size() == 0)
        throw std::runtime_error("unable to open file");

    spirv_cross::CompilerGLSL comp(std::move(spirv));

    auto resources = comp.get_shader_resources();

    REQUIRE(resources.uniform_buffers[0].name == "UniformBufferObject");
}