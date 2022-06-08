set(CROSS_LIBS spirv-cross-c spirv-cross-core spirv-cross-cpp spirv-cross-glsl
    spirv-cross-hlsl spirv-cross-msl spirv-cross-reflect spirv-cross-util)

if(WIN32)
    set(INC_DIR "Include")
    set(LIB_DIR "Lib")
    set(BIN_DIR "Bin")
else()
    set(INCLUDE_DIR "include")
    set(LIB_DIR "lib")
    set(bin_dir "bin")
endif()

find_path(SPIRVCROSS_INCLUDE_DIRS spirv_cross/spirv_cross.hpp
    HINTS
        "$ENV{VULKAN_SDK}/${INC_DIR}")

set(CROSS_LIB_PATHS)
foreach(CROSS_LIB ${CROSS_LIBS})
    find_library(CROSS_${CROSS_LIB} ${CROSS_LIB}
        HINTS
            "$ENV{VULKAN_SDK}/${LIB_DIR}"
            "$ENV{VULKAN_SDK}/${BIN_DIR}")

    list(APPEND CROSS_LIB_PATHS CROSS_${CROSS_LIB})
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SPIRVCROSS DEFAULT_MSG
    SPIRVCROSS_INCLUDE_DIRS
    ${CROSS_LIB_PATHS})

if(SPIRVCROSS_FOUND)
    set(SPIRVCROSS_LIBRARIES)
    foreach(CROSS_LIB_PATH ${CROSS_LIB_PATHS})
        list(APPEND SPIRVCROSS_LIBRARIES ${${CROSS_LIB_PATH}})
    endforeach()
endif()