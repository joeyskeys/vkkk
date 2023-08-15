#include <catch2/catch_all.hpp>

#include "concepts/mesh.h"

using Catch::Approx;

TEST_CASE("Vertex test", "[single-file]") {
    vkkk::Vert v{0, 1, 2};
    REQUIRE(v.x == Approx(0.f));
    REQUIRE(v.arr[1] == Approx(1.f));
}