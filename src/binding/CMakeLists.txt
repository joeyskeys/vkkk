find_package(Python 3.8 COMPONENTS Interpreter Development.Module REQUIRED)

set(SRC_FILES
    api.cpp
    binding.cpp
    types.cpp)

nanobind_add_module(pyvkkk ${SRC_FILES})
target_link_libraries(pyvkkk PRIVATE
    vkkk)