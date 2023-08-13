#include <catch2/catch_all.hpp>

#include "utils/singleton.h"

class Test : public vkkk::Singleton<Test> {
public:
    int val;

private:
    friend class vkkk::Singleton<Test>;

    Test(int v) : val(v) {}
};

TEST_CASE("Singleton test", "[single-file]") {
    auto test_ins = Test::instance(1);
    REQUIRE(test_ins.val == 1);
}