find_path(OIIO_INCLUDE_DIRS OpenImageIO/imageio.h
    HINTS
        /usr
        /usr/local
    PATH_SUFFIXES
        include)

if (CMAKE_BUILD_TYPE EQUAL "Debug")
    set(OIIO_LIB_NAME "OpenImageIO_d")
    set(OIIO_UTIL_NAME "OpenImageIO_Util_d")
else()
    set(OIIO_LIB_NAME "OpenImageIO")
    set(OIIO_UTIL_NAME "OpenImageIO_Util")
endif()

find_library(OIIO_LIB ${OIIO_LIB_NAME}
    HINTS
        /usr
        /usr/local
    PATH_SUFFIXES
        lib)

find_library(OIIO_UTIL_LIB ${OIIO_UTIL_NAME}
    HINTS
        /usr
        /usr/local
    PATH_SUFFIXES
        lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OIIO DEFAULT_MSG
    OIIO_INCLUDE_DIRS
    OIIO_LIB
    OIIO_UTIL_LIB)

if(OIIO_FOUND)
    set(OIIO_LIBRARIES ${OIIO_LIB} ${OIIO_UTIL_LIB})
endif()