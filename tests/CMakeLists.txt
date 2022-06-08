find_package(Catch2 REQUIRED)

add_executable(resource_test cross_tests/resource_test.cpp)
target_link_libraries(resource_test
    PRIVATE
        Catch2::Catch2WithMain
        ${SPIRVCROSS_LIBRARIES})