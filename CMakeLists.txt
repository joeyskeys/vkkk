cmake_minimum_required(VERSION 3.18)

cmake_policy(SET CMP0074 NEW)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "set build type")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)

project(vkkk)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optional vcpkg package root for dependencies installed under installed/x64-windows.
# Set VKKK_VCPKG_INSTALLED_DIR on machines where vcpkg is installed in a different location.
set(_vkkk_vcpkg_default "")
if(DEFINED ENV{VCPKG_ROOT})
    set(_vkkk_vcpkg_default "$ENV{VCPKG_ROOT}/installed/x64-windows")
endif()
set(VKKK_VCPKG_INSTALLED_DIR "${_vkkk_vcpkg_default}" CACHE PATH "Path to vcpkg installed triplet directory")
unset(_vkkk_vcpkg_default)
if(VKKK_VCPKG_INSTALLED_DIR)
    list(PREPEND CMAKE_PREFIX_PATH "${VKKK_VCPKG_INSTALLED_DIR}" "${VKKK_VCPKG_INSTALLED_DIR}/share")
    # Prefer vcpkg headers over SDK headers to avoid ABI mismatches
    # (e.g. spirv-cross headers from Vulkan SDK vs vcpkg libraries).
    include_directories(BEFORE SYSTEM "${VKKK_VCPKG_INSTALLED_DIR}/include")
endif()

option(VKKK_BUILD_PYTHON_BINDINGS "Build Python bindings (nanobind module)" ON)

# Optional Python location for nanobind bindings.
# For Conda, set this to your environment prefix (or rely on CONDA_PREFIX default).
set(_vkkk_python_root_default "")
if(DEFINED ENV{CONDA_PREFIX})
    set(_vkkk_python_root_default "$ENV{CONDA_PREFIX}")
endif()
set(VKKK_PYTHON_ROOT_DIR "${_vkkk_python_root_default}" CACHE PATH "Root directory of Python installation")
unset(_vkkk_python_root_default)
set(VKKK_PYTHON_EXECUTABLE "" CACHE FILEPATH "Explicit path to Python executable (optional)")
if(VKKK_PYTHON_ROOT_DIR)
    set(Python_ROOT_DIR "${VKKK_PYTHON_ROOT_DIR}")
endif()
if(VKKK_PYTHON_EXECUTABLE)
    set(Python_EXECUTABLE "${VKKK_PYTHON_EXECUTABLE}")
endif()

find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(OpenImageIO CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(minizip-ng CONFIG REQUIRED)

# Use Vulkan SDK-provided glm / SPIRV-Cross / shaderc.
set(VKKK_VULKAN_SDK_DIR "$ENV{VULKAN_SDK}" CACHE PATH "Vulkan SDK root directory")
if(NOT VKKK_VULKAN_SDK_DIR)
    if(DEFINED Vulkan_INCLUDE_DIR AND Vulkan_INCLUDE_DIR)
        get_filename_component(VKKK_VULKAN_SDK_DIR "${Vulkan_INCLUDE_DIR}" DIRECTORY)
    elseif(DEFINED Vulkan_INCLUDE_DIRS AND Vulkan_INCLUDE_DIRS)
        list(GET Vulkan_INCLUDE_DIRS 0 _vkkk_vk_inc0)
        get_filename_component(VKKK_VULKAN_SDK_DIR "${_vkkk_vk_inc0}" DIRECTORY)
        unset(_vkkk_vk_inc0)
    endif()
endif()

if(NOT VKKK_VULKAN_SDK_DIR OR NOT EXISTS "${VKKK_VULKAN_SDK_DIR}/Lib")
    message(FATAL_ERROR "Vulkan SDK root not found. Set VKKK_VULKAN_SDK_DIR or VULKAN_SDK.")
endif()

include_directories(BEFORE SYSTEM "${VKKK_VULKAN_SDK_DIR}/Include")

add_library(VKKK::glm INTERFACE IMPORTED)
set_property(TARGET VKKK::glm PROPERTY
    INTERFACE_INCLUDE_DIRECTORIES "${VKKK_VULKAN_SDK_DIR}/Include")

add_library(VKKK::spirv_cross INTERFACE IMPORTED)
set_property(TARGET VKKK::spirv_cross PROPERTY INTERFACE_LINK_LIBRARIES
    "$<$<CONFIG:Debug>:${VKKK_VULKAN_SDK_DIR}/Lib/spirv-cross-glsld.lib>"
    "$<$<CONFIG:Debug>:${VKKK_VULKAN_SDK_DIR}/Lib/spirv-cross-cored.lib>"
    "$<$<NOT:$<CONFIG:Debug>>:${VKKK_VULKAN_SDK_DIR}/Lib/spirv-cross-glsl.lib>"
    "$<$<NOT:$<CONFIG:Debug>>:${VKKK_VULKAN_SDK_DIR}/Lib/spirv-cross-core.lib>")

add_library(VKKK::shaderc INTERFACE IMPORTED)
set_property(TARGET VKKK::shaderc PROPERTY INTERFACE_LINK_LIBRARIES
    "$<$<CONFIG:Debug>:${VKKK_VULKAN_SDK_DIR}/Lib/shaderc_combinedd.lib>"
    "$<$<NOT:$<CONFIG:Debug>>:${VKKK_VULKAN_SDK_DIR}/Lib/shaderc_combined.lib>")

include_directories(${CMAKE_SOURCE_DIR}/thirdparty/stb)
include_directories(${CMAKE_SOURCE_DIR}/src)

if(VKKK_BUILD_PYTHON_BINDINGS)
    find_package(Python 3.8 COMPONENTS Interpreter Development.Module REQUIRED)
    add_subdirectory(thirdparty/nanobind)
endif()
add_subdirectory(src)
add_subdirectory(plugin)
add_subdirectory(tests)
