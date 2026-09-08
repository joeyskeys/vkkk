cmake_minimum_required(VERSION 3.18)

cmake_policy(SET CMP0074 NEW)
cmake_policy(SET CMP0144 NEW)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "set build type")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)

project(vkkk)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    # MSVC reports __cplusplus as 199711L unless this is set. Qt6 checks that
    # macro and rejects the compiler even when /std:c++20 is already on.
    add_compile_options(/Zc:__cplusplus)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

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
find_package(glfw3 CONFIG QUIET)
find_package(fmt CONFIG REQUIRED)
find_package(OpenImageIO CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(minizip-ng CONFIG REQUIRED)
find_package(Freetype REQUIRED)

set(SHADERC_ROOT ${VKKK_VCPKG_INSTALLED_DIR})
set(SPIRV_CROSS_ROOT ${VKKK_VCPKG_INSTALLED_DIR})

find_package(shaderc REQUIRED)
find_package(spirv_cross REQUIRED)
find_package(glm CONFIG REQUIRED)

set(VKKK_HAS_GLFW OFF)
if(glfw3_FOUND)
    set(VKKK_HAS_GLFW ON)
    message(STATUS "vkkk: GLFW window backend enabled")
else()
    message(STATUS "vkkk: GLFW not found; GlfwBackend disabled")
endif()

# When the current build type has no matching imported config (common with
# Qt kits that ship only Release, or no RelWithDebInfo), fall back instead of
# dropping the .lib from the link line.
set(CMAKE_MAP_IMPORTED_CONFIG_DEBUG DEBUG RELEASE)
set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO RELWITHDEBINFO RELEASE)
set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL MINSIZEREL RELEASE)

find_package(Qt6 QUIET COMPONENTS Core Gui Widgets)
set(VKKK_HAS_QT6 OFF)
if(TARGET Qt6::Core AND TARGET Qt6::Gui AND TARGET Qt6::Widgets)
    set(VKKK_HAS_QT6 ON)
    set(CMAKE_AUTOMOC ON)
    message(STATUS "vkkk: Qt6 ${Qt6_VERSION} window backend enabled")
    if(Qt6Core_INCLUDE_DIRS)
        # vcpkg include/ is prepended globally and can shadow a custom Qt6_DIR.
        include_directories(BEFORE SYSTEM
            ${Qt6Core_INCLUDE_DIRS}
            ${Qt6Gui_INCLUDE_DIRS}
            ${Qt6Widgets_INCLUDE_DIRS})
    endif()
else()
    message(STATUS "vkkk: Qt6 not found; QtBackend disabled")
endif()

if(NOT VKKK_HAS_GLFW AND NOT VKKK_HAS_QT6)
    message(WARNING "vkkk: no window backend found (install GLFW and/or Qt6)")
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/stb)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui/backends)

if(VKKK_BUILD_PYTHON_BINDINGS)
    find_package(Python 3.8 COMPONENTS Interpreter Development.Module REQUIRED)
    add_subdirectory(thirdparty/nanobind)
endif()
add_subdirectory(src)
add_subdirectory(examples)
add_subdirectory(plugin)
#add_subdirectory(tests)
