set(HEADERS
    built_in_shader/built_in_shader_mgr.h
    built_in_shader/fixed_color.h
    built_in_shader/phong.h
    built_in_shader/pbr.h
    asset_mgr/scene.h
    asset_mgr/light_mgr.h
    asset_mgr/drawable_mgr.h
    concepts/camera.h
    concepts/line.h
    concepts/mesh.h
    gui/gui.h
    renderer/renderer.h
    renderer/forward.h
    renderer/deferred.h
    utils/common.h
    utils/io.h
    utils/singleton.h
    vk_ins/cmd_buf.h
    vk_ins/misc.h
    vk_ins/vkabstraction.h
    vk_ins/shader_mgr.h)

set(SRCS
    ../thirdparty/imgui/imgui.cpp
    ../thirdparty/imgui/imgui_draw.cpp
    ../thirdparty/imgui/imgui_tables.cpp
    ../thirdparty/imgui/imgui_widgets.cpp
    ../thirdparty/imgui/backends/imgui_impl_glfw.cpp
    ../thirdparty/imgui/backends/imgui_impl_vulkan.cpp
    built_in_shader/built_in_shader_mgr.cpp
    asset_mgr/scene.cpp
    asset_mgr/light_mgr.cpp
    asset_mgr/drawable_mgr.cpp
    concepts/camera.cpp
    concepts/line.cpp
    concepts/mesh.cpp
    gui/gui.cpp
    vk_ins/cmd_buf.cpp
    vk_ins/misc.cpp
    vk_ins/vkabstraction.cpp
    vk_ins/shader_mgr.cpp)

include_directories(${CMAKE_SOURCE_DIR}/thirdparty/imgui)
include_directories(${CMAKE_SOURCE_DIR}/thirdparty/imgui/backends)

add_library(vkkk ${HEADERS} ${SRCS})
target_link_libraries(vkkk
    PUBLIC
        Vulkan::Vulkan
        glfw
        fmt::fmt
        OpenImageIO::OpenImageIO
        assimp::assimp
        ${SPIRV_CROSS_LIBRARIES})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_libraries(vkkk PRIVATE ${SHADERC_DEBUG_LIBRARIES})
else()
    target_link_libraries(vkkk PRIVATE ${SHADERC_RELEASE_LIBRARIES})
endif()

add_executable(vkmain main.cpp)
target_link_libraries(vkmain
    PRIVATE
        vkkk
        OpenImageIO::OpenImageIO)

add_executable(vkcornell cornell_main.cpp)
target_link_libraries(vkcornell
    PRIVATE
        vkkk
        OpenImageIO::OpenImageIO)

add_executable(vkdebug debug_main.cpp)
target_link_libraries(vkdebug
    PRIVATE
        vkkk
        OpenImageIO::OpenImageIO)

if(VKKK_BUILD_PYTHON_BINDINGS)
    add_subdirectory(binding)
endif()