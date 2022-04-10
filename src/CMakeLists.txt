set(HEADERS
    gui/gui.h
    vk_ins/vkabstraction.h)

set(SRCS
    gui/gui.cpp
    vk_ins/vkabstraction.cpp)

add_library(vkkk ${HEADERS} ${SRCS})
target_link_libraries(vkkk
    ${GLFW_LIBRARIES}
    ${Vulkan_LIBRARIES}
    ${OIIO_LIBRARIES})

add_executable(vktest main.cpp)
target_link_libraries(vktest
    vkkk)