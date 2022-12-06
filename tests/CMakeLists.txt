find_package(Catch2 REQUIRED)

add_executable(resource_test cross_tests/resource_test.cpp)
target_link_libraries(resource_test
    PRIVATE
        Catch2::Catch2WithMain
        ${SPIRVCROSS_LIBRARIES})

add_executable(texture_test texture_tests/texture_test.cpp)
target_link_libraries(texture_test
    PRIVATE
        Catch2::Catch2WithMain
        vkkk)