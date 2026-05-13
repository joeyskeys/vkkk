cmake_minimum_required(VERSION 3.18)

cmake_policy(SET CMP0074 NEW)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "set build type")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)

project(vkkk)

# Conan CMakeToolchain pins compiler cppstd from profile (often 14); this project uses C++20.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Conan CMakeDeps writes *-config.cmake into the output folder (usually ${CMAKE_BINARY_DIR}). If the preset's
# toolchain path does not point at build/conan_toolchain.cmake, still allow find_package to see those files.
list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}")
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}")

option(VKKK_BUILD_PYTHON_BINDINGS "Build Python bindings (nanobind module)" ON)

# Dependencies via Conan CMakeDeps (also pass -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake from repo root).
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(fmt REQUIRED)
# OpenColorIO (OIIO dependency) expects MINIZIP::minizip; Conan exposes minizip::minizip.
find_package(minizip REQUIRED)
if(NOT TARGET MINIZIP::minizip AND TARGET minizip::minizip)
    add_library(MINIZIP::minizip INTERFACE IMPORTED)
    set_property(TARGET MINIZIP::minizip PROPERTY INTERFACE_LINK_LIBRARIES minizip::minizip)
endif()
find_package(OpenImageIO REQUIRED)
find_package(assimp REQUIRED)
find_package(spirv-cross REQUIRED)
find_package(shaderc REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/thirdparty/stb)
include_directories(${CMAKE_SOURCE_DIR}/src)

if(VKKK_BUILD_PYTHON_BINDINGS)
    add_subdirectory(thirdparty/nanobind)
endif()
add_subdirectory(src)
add_subdirectory(plugin)
add_subdirectory(tests)
