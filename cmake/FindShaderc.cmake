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

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SHADERC DEFAULT_MSG
    SHADERC_INCLUDE_DIR
    SHADERC_LIB)

if (SHADER_FOUND)
    set(SHADERC_INCLUDE_DIRS ${SHADERC_INCLUDE_DIR})
    set(SHADERC_LIBRARIES ${SHADERC_LIB})
endif()