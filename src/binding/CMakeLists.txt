if(DEFINED ENV{CONDA_PREFIX})
    # Prefer the active Conda environment on Windows where Python is often not installed system-wide.
    if(NOT DEFINED Python_ROOT_DIR)
        set(Python_ROOT_DIR "$ENV{CONDA_PREFIX}" CACHE PATH "Root path to Python installation")
    endif()
    if(NOT DEFINED Python_EXECUTABLE)
        set(Python_EXECUTABLE "$ENV{CONDA_PREFIX}/python.exe" CACHE FILEPATH "Python interpreter path")
    endif()
    set(Python_FIND_REGISTRY NEVER)
    set(Python_FIND_STRATEGY LOCATION)
endif()

find_package(Python 3.8 COMPONENTS Interpreter Development.Module REQUIRED)

set(SRC_FILES
    api.cpp
    binding.cpp
    math.cpp
    types.cpp)

nanobind_add_module(pyvkkk ${SRC_FILES})
target_link_libraries(pyvkkk PRIVATE
    vkkk
    OpenImageIO::OpenImageIO)

# OpenColorIO symbols from OpenImageIO require minizip-ng on vcpkg static linking.
target_link_libraries(pyvkkk PRIVATE MINIZIP::minizip-ng)

add_custom_command(TARGET pyvkkk POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:pyvkkk>
        ${CMAKE_SOURCE_DIR}/plugin/vulender)

add_custom_target(copylib ALL
    DEPENDS pyvkkk)
