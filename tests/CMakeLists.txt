find_package(Catch2 REQUIRED)

add_executable(resource_test cross_tests/resource_test.cpp)
target_link_libraries(resource_test
    PRIVATE
        Catch2::Catch2WithMain
        ${SPIRVCROSS_LIBRARIES})

add_executable(texture_test uniform_tests/texture_test.cpp)
target_link_libraries(texture_test
    PRIVATE
        Catch2::Catch2WithMain
        vkkk)

add_executable(mgr_test uniform_tests/mgr_test.cpp)
target_link_libraries(mgr_test
    PRIVATE
        Catch2::Catch2WithMain
        vkkk)

add_executable(singleton_test language_tests/singleton_test.cpp)
target_link_libraries(singleton_test
    PRIVATE
        Catch2::Catch2WithMain
        vkkk)

add_executable(shadermgr_test asset_mgr_tests/shader_mgr_test.cpp)
target_link_libraries(shadermgr_test
    PRIVATE
        Catch2::Catch2WithMain
        vkkk)