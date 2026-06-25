set(HEADERS
    built_in_shader/built_in_shader_mgr.h
    built_in_shader/fixed_color.h
    built_in_shader/phong.h
    built_in_shader/pbr.h
    built_in_shader/shadow.h
    built_in_shader/shadow.h
    asset_mgr/scene.h
    asset_mgr/light_mgr.h
    asset_mgr/drawable_mgr.h
    concepts/camera.h
    concepts/line.h
    concepts/mesh.h
    gui/gui.h
    renderer/renderer.hpp
    renderer/forward.h
    renderer/forward_plus.h
    renderer/deferred.h
    utils/common.h
    utils/io.h
    utils/singleton.h
    vk_ins/cmd_buf.h
    vk_ins/misc.h
    vk_ins/compute_shader.hpp
    #vk_ins/vkabstraction.h
    #vk_ins/shader_mgr.h
    vk_ins/shader_module_pack.hpp
    vk_ins/context.hpp
    vk_ins/instance_attr.hpp)

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
    renderer/forward.cpp
    renderer/forward_plus.cpp
    vk_ins/cmd_buf.cpp
    vk_ins/misc.cpp
    vk_ins/compute_shader.cpp
    #vk_ins/vkabstraction.cpp
    #vk_ins/shader_mgr.cpp
    vk_ins/shader_module_pack.cpp
    vk_ins/context.cpp
    vk_ins/context_glfw.cpp
    vk_ins/context_utils.cpp)

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

#add_executable(vkmain main.cpp)
#target_link_libraries(vkmain
    #PRIVATE
        #vkkk
        #OpenImageIO::OpenImageIO)

#add_executable(vkcornell cornell_main.cpp)
#target_link_libraries(vkcornell
    #PRIVATE
        #vkkk
        #OpenImageIO::OpenImageIO)

add_executable(vkcornellpp cornal_mainpp.cpp)
target_link_libraries(vkcornellpp
    PRIVATE
        vkkk
        OpenImageIO::OpenImageIO)

add_executable(vkrendererpp renderer_mainpp.cpp)
target_link_libraries(vkrendererpp
    PRIVATE
        vkkk
        OpenImageIO::OpenImageIO)

add_executable(vkcornellpluspp cornell_main_plus.cpp)
target_link_libraries(vkcornellpluspp
    PRIVATE
        vkkk
        OpenImageIO::OpenImageIO)

add_executable(vkrendererforwardplus renderer_main_forwardplus.cpp)
target_link_libraries(vkrendererforwardplus
    PRIVATE
        vkkk
        OpenImageIO::OpenImageIO)

add_executable(vkrendererfp renderer_main_fp.cpp)
target_link_libraries(vkrendererfp
    PRIVATE
        vkkk
        OpenImageIO::OpenImageIO)

#add_executable(vkrenderer renderer_main.cpp)
#target_link_libraries(vkrenderer
    #PRIVATE
        #vkkk
        #OpenImageIO::OpenImageIO)

#add_executable(vktest test_main.cpp)
#target_link_libraries(vktest
    #PUBLIC
        #Vulkan::Vulkan)

#if(VKKK_BUILD_PYTHON_BINDINGS)
    #add_subdirectory(binding)
#endif()