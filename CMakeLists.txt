cmake_minimum_required(VERSION 3.10)

cmake_policy(SET CMP0074 NEW)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

project(vkkk)

find_package(Vulkan)

if(NOT Vulkan_FOUND)
    message(FATAL_ERROR "Could not find Vulkan library!")
else()
    message(STATUS "Found Vulkan: ${Vulkan_LIBRARY}")
endif()

find_package(GLFW)

if(NOT GLFW_FOUND)
    message(FATAL_ERROR "Could not find GLFW library!")
else()
    message(STATUS "Found GLFW: ${GLFW_LIBRARY}")
endif()

find_package(GLM)

if (NOT GLM_FOUND)
    message(FATAL_ERROR "Could not find GLM library!")
else()
    message(STATUS "Found GLM: ${GLM_INCLUDE_DIRS}")
endif()

find_package(OIIO)

if (NOT OIIO_FOUND)
    message(FATAL_ERROR "Could not find oiio library!")
else()
    message(STATUS "Found oiio: ${OIIO_INCLUDE_DIRS}")
endif()

include_directories(${Vulkan_INCLUDE_DIRS})
include_directories(${GLFW_INCLUDE_DIRS})
include_directories(${OIIO_INCLUDE_DIRS})

add_executable(vkkk main.cpp vkabstraction.cpp gui.cpp)
target_link_libraries(vkkk
    ${GLFW_LIBRARIES}
    ${Vulkan_LIBRARIES}
    ${OIIO_LIBRARIES})