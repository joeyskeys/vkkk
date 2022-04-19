set(HEADERS
    asset_mgr/mesh.h
    asset_mgr/mesh_mgr.h
    gui/gui.h
    utils/common.h
    utils/io.h
    utils/singleton.h
    vk_ins/shader_mgr.h
    vk_ins/vkabstraction.h)

set(SRCS
    asset_mgr/mesh.cpp
    asset_mgr/mesh_mgr.cpp
    gui/gui.cpp
    vk_ins/shader_mgr.cpp
    vk_ins/vkabstraction.cpp)

add_library(vkkk ${HEADERS} ${SRCS})
target_link_libraries(vkkk
    ${GLFW_LIBRARIES}
    ${Vulkan_LIBRARIES}
    ${OIIO_LIBRARIES}
    ${ASSIMP_LIBRARIES})

add_executable(vktest main.cpp)
target_link_libraries(vktest
    vkkk)