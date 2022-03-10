find_path(GLM_INCLUDE_DIR glm/glm.hpp
    HINTS
        /usr
        /usr/local
        ${GLM_ROOT}
    PATH_SUFFIXES
        include)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GLM DEFAULT_MSG
    GLM_INCLUDE_DIR)

if (GLM_FOUND)
    set(GLM_INCLUDE_DIRS ${GLM_INCLUDE_DIR})
endif()