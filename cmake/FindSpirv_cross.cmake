set(_SPIRV_CROSS_HINTS
    /usr
    /usr/local
    ${SPIRV_CROSS_ROOT}
    $ENV{SPIRV_CROSS_ROOT})

unset(SPIRV_CROSS_C_LIB CACHE)
unset(SPIRV_CROSS_C_LIB_DEBUG CACHE)
unset(SPIRV_CROSS_CORE_LIB CACHE)
unset(SPIRV_CROSS_CORE_LIB_DEBUG CACHE)
unset(SPIRV_CROSS_GLSL_LIB CACHE)
unset(SPIRV_CROSS_GLSL_LIB_DEBUG CACHE)
unset(SPIRV_CROSS_REFLECT_LIB CACHE)
unset(SPIRV_CROSS_REFLECT_LIB_DEBUG CACHE)
unset(SPIRV_CROSS_UTIL_LIB CACHE)
unset(SPIRV_CROSS_UTIL_LIB_DEBUG CACHE)

find_path(SPIRV_CROSS_INCLUDE_DIR spirv_cross/spirv_cross.hpp
    HINTS ${_SPIRV_CROSS_HINTS}
    PATH_SUFFIXES include)

find_library(SPIRV_CROSS_C_LIB
    NAMES spirv-cross-c
    HINTS ${_SPIRV_CROSS_HINTS}
    PATH_SUFFIXES lib)
find_library(SPIRV_CROSS_C_LIB_DEBUG
    NAMES spirv-cross-cd spirv-cross-c
    HINTS ${_SPIRV_CROSS_HINTS}
    PATH_SUFFIXES debug/lib lib)

find_library(SPIRV_CROSS_CORE_LIB
    NAMES spirv-cross-core
    HINTS ${_SPIRV_CROSS_HINTS}
    PATH_SUFFIXES lib)
find_library(SPIRV_CROSS_CORE_LIB_DEBUG
    NAMES spirv-cross-cored spirv-cross-core
    HINTS ${_SPIRV_CROSS_HINTS}
    PATH_SUFFIXES debug/lib lib)

find_library(SPIRV_CROSS_GLSL_LIB
    NAMES spirv-cross-glsl
    HINTS ${_SPIRV_CROSS_HINTS}
    PATH_SUFFIXES lib)
find_library(SPIRV_CROSS_GLSL_LIB_DEBUG
    NAMES spirv-cross-glsld spirv-cross-glsl
    HINTS ${_SPIRV_CROSS_HINTS}
    PATH_SUFFIXES debug/lib lib)

find_library(SPIRV_CROSS_REFLECT_LIB
    NAMES spirv-cross-reflect
    HINTS ${_SPIRV_CROSS_HINTS}
    PATH_SUFFIXES lib)
find_library(SPIRV_CROSS_REFLECT_LIB_DEBUG
    NAMES spirv-cross-reflectd spirv-cross-reflect
    HINTS ${_SPIRV_CROSS_HINTS}
    PATH_SUFFIXES debug/lib lib)

find_library(SPIRV_CROSS_UTIL_LIB
    NAMES spirv-cross-util
    HINTS ${_SPIRV_CROSS_HINTS}
    PATH_SUFFIXES lib)
find_library(SPIRV_CROSS_UTIL_LIB_DEBUG
    NAMES spirv-cross-utild spirv-cross-util
    HINTS ${_SPIRV_CROSS_HINTS}
    PATH_SUFFIXES debug/lib lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(spirv_cross DEFAULT_MSG
    SPIRV_CROSS_INCLUDE_DIR
    SPIRV_CROSS_CORE_LIB
    SPIRV_CROSS_GLSL_LIB)

macro(_vkkk_append_configured_lib release_var debug_var)
    if(DEFINED ${debug_var} AND NOT "${${debug_var}}" STREQUAL "")
        list(APPEND SPIRV_CROSS_LIBRARIES debug "${${debug_var}}")
    elseif(DEFINED ${release_var} AND NOT "${${release_var}}" STREQUAL "")
        list(APPEND SPIRV_CROSS_LIBRARIES debug "${${release_var}}")
    endif()

    if(DEFINED ${release_var} AND NOT "${${release_var}}" STREQUAL "")
        list(APPEND SPIRV_CROSS_LIBRARIES optimized "${${release_var}}")
    elseif(DEFINED ${debug_var} AND NOT "${${debug_var}}" STREQUAL "")
        list(APPEND SPIRV_CROSS_LIBRARIES optimized "${${debug_var}}")
    endif()
endmacro()

if(SPIRV_CROSS_FOUND)
    set(SPIRV_CROSS_INCLUDE_DIRS "${SPIRV_CROSS_INCLUDE_DIR}")
    set(SPIRV_CROSS_LIBRARIES "")

    _vkkk_append_configured_lib(SPIRV_CROSS_C_LIB SPIRV_CROSS_C_LIB_DEBUG)
    _vkkk_append_configured_lib(SPIRV_CROSS_CORE_LIB SPIRV_CROSS_CORE_LIB_DEBUG)
    _vkkk_append_configured_lib(SPIRV_CROSS_GLSL_LIB SPIRV_CROSS_GLSL_LIB_DEBUG)
    _vkkk_append_configured_lib(SPIRV_CROSS_REFLECT_LIB SPIRV_CROSS_REFLECT_LIB_DEBUG)
    _vkkk_append_configured_lib(SPIRV_CROSS_UTIL_LIB SPIRV_CROSS_UTIL_LIB_DEBUG)
endif()