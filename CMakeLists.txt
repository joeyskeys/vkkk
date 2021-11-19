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

include_directories(${Vulkan_INCLUDE_DIRS})
include_directories(${GLFW_INCLUDE_DIRS})

add_executable(vkkk main.cpp vkabstraction.cpp gui.cpp)