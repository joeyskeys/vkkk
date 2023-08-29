find_path(SHADERC_INCLUDE_DIR shaderc/shaderc.h
    HINTS
        /usr
        /usr/local
        ${SHADERC_ROOT}
    PATH_SUFFIXES
        include)

find_library(SHADERC_LIB shaderc_combined
    HINTS
        /usr
        /usr/local
        ${SHADERC_ROOT}
    PATH_SUFFIXES
        lib)

find_library(GLSLANG_LIB glslang
    HINTS
        /usr
        /usr/local
        ${SHADERC_ROOT}
    PATH_SUFFIXES
        lib)

find_library(OGLCOMPILER_LIB OGLCompiler
    HINTS
        /usr
        /usr/local
        ${SHADERC_ROOT}
    PATH_SUFFIXES
        lib)

find_library(SPIRV_LIB SPIRV
    HITNS
        /usr
        /usr/local
        ${SHADERC_ROOT}
    PATH_SUFFIXES
        lib)

find_library(SPIRVTOOLS_LIB SPIRV-Tools
    HINTS
        /usr
        /usr/local
        ${SHADERC_ROOT}
    PATH_SUFFIXES
        lib)

find_library(SPIRVTOOLSOPT_LIB SPIRV-Tools-opt
    HINTS
        /usr
        /usr/local
        ${SHADERC_ROOT}
    PATH_SUFFIXES
        lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SHADERC DEFAULT_MSG
    SHADERC_INCLUDE_DIR
    SHADERC_LIB
    GLSLANG_LIB
    OGLCOMPILER_LIB
    SPIRV_LIB
    SPIRVTOOLS_LIB
    SPIRVTOOLSOPT_LIB)

if (SHADERC_FOUND)
    set(SHADERC_INCLUDE_DIRS ${SHADERC_INCLUDE_DIR})
    set(SHADERC_LIBRARIES ${SHADERC_LIB} ${GLSLANG_LIB}
        ${OGLCOMPILER_LIB} ${SPIRV_LIB} ${SPIRVTOOLS_LIB}
        ${SPIRVTOOLSOPT_LIB})
endif()