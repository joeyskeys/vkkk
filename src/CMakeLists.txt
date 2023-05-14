set(HEADERS
    asset_mgr/mesh.h
    asset_mgr/mesh_mgr.h
    concepts/camera.h
    gui/gui.h
    utils/common.h
    utils/io.h
    utils/singleton.h
    vk_ins/misc.h
    vk_ins/shader_mgr.h
    vk_ins/uniform_mgr.h
    vk_ins/vkabstraction.h
    vk_ins/vktexture.h
    vk_ins/vkubo.h)

set(SRCS
    asset_mgr/mesh.cpp
    asset_mgr/mesh_mgr.cpp
    concepts/camera.cpp
    gui/gui.cpp
    vk_ins/misc.cpp
    vk_ins/shader_mgr.cpp
    vk_ins/uniform_mgr.cpp
    vk_ins/vkabstraction.cpp
    vk_ins/vktexture.cpp
    vk_ins/vkubo.cpp)

add_library(vkkk ${HEADERS} ${SRCS})
set(PLATFORM_RELATED_LIBS)
if (UNIX)
    set(PLATFORM_RELATED_LIBS ${X11_LIBRARIES})
endif()

target_link_libraries(vkkk
    ${GLFW_LIBRARIES}
    ${Vulkan_LIBRARIES}
    ${OIIO_LIBRARIES}
    ${ASSIMP_LIBRARIES}
    ${SPIRVCROSS_LIBRARIES}
    ${PLATFORM_RELATED_LIBS})

add_executable(vktest main.cpp)
target_link_libraries(vktest
    vkkk
    ${OIIO_LIBRARIES})