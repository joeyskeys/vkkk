find_path(ASSIMP_INCLUDE_DIRS assimp/Importer.hpp
    HINTS
        /usr
        /usr/local
        ${ASSIMP_ROOT}
        $ENV{ASSIMP_ROOT}
    PATH_SUFFIXES
        include)

find_library(ASSIMP_LIB assimp
    HINTS
        /usr
        /usr/local
        ${ASSIMP_ROOT}
        $ENV{ASSIMP_ROOT}
    PATH_SUFFIXES
        lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ASSIMP DEFAULT_MSG
    ASSIMP_INCLUDE_DIRS
    ASSIMP_LIB)

if(ASSIMP_FOUND)
    set(ASSIMP_LIBRARIES ${ASSIMP_LIB})
endif()