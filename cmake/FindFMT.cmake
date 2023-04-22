find_path(FMT_INCLUDE_DIRS fmt/core.h
    HINTS
        /usr
        /usr/local
        ${FMT_ROOT}
        $ENV{FMT_ROOT}
    PATH_SUFFIXES
        include)

find_library(FMT_LIBRARY fmt
    HINTS
        /usr
        /usr/local
        ${FMT_ROOT}
        $ENV{FMT_ROOT}
    PATH_SUFFIXES
        lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FMT DEFAULT_MSG
    FMT_INCLUDE_DIRS
    FMT_LIBRARY)

if(FMT_FOUND)
    set(FMT_LIBRARIES ${FMT_LIBRARY})
endif()