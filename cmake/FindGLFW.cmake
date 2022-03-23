find_path(GLFW_INCLUDE_DIR GLFW/glfw3.h
    HINTS
        /usr
        /usr/local
        ${GLFW_ROOT}
    PATH_SUFFIXES
        include)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(LIB_NAME glfw)
else()
    set(LIB_NAME glfw3)
endif()

find_library(GLFW_LIB ${LIB_NAME}
    HINTS
        /usr
        /usr/local
        ${GLFW_ROOT}
    PATH_SUFFIXES
        lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GLFW DEFAULT_MSG
    GLFW_INCLUDE_DIR
    GLFW_LIB)

if(GLFW_FOUND)
    set(GLFW_INCLUDE_DIRS ${GLFW_INCLUDE_DIR})
    set(GLFW_LIBRARIES ${GLFW_LIB})
endif()
