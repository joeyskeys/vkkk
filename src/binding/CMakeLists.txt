find_package(Python 3.8 COMPONENTS Interpreter Development.Module REQUIRED)

set(SRC_FILES
    api.cpp
    binding.cpp
    math.cpp
    types.cpp)

nanobind_add_module(pyvkkk ${SRC_FILES})
target_link_libraries(pyvkkk PRIVATE
    vkkk
    openimageio::openimageio)

# OpenColorIO symbols from OpenImageIO may require explicit minizip on Windows static linking.
if(TARGET MINIZIP::minizip)
    target_link_libraries(pyvkkk PRIVATE MINIZIP::minizip)
elseif(TARGET minizip::minizip)
    target_link_libraries(pyvkkk PRIVATE minizip::minizip)
endif()

add_custom_command(TARGET pyvkkk POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:pyvkkk>
        ${CMAKE_SOURCE_DIR}/plugin/vulender)

add_custom_target(copylib ALL
    DEPENDS pyvkkk)
