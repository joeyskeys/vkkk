add_executable(vkrendererforward rendering_feature/renderer_main_forward.cpp)
target_link_libraries(vkrendererforward
    PRIVATE
        vkkk
        OpenImageIO::OpenImageIO)

add_executable(vkrendererfp rendering_feature/renderer_main_fp.cpp)
target_link_libraries(vkrendererfp
    PRIVATE
        vkkk
        OpenImageIO::OpenImageIO)

add_executable(vkrendererforwardplus rendering_feature/renderer_main_forwardplus.cpp)
target_link_libraries(vkrendererforwardplus
    PRIVATE
        vkkk
        OpenImageIO::OpenImageIO)

add_executable(vkrendererwireframe rendering_feature/renderer_main_wireframe.cpp)
target_link_libraries(vkrendererwireframe
    PRIVATE
        vkkk
        OpenImageIO::OpenImageIO)

# add_executable(api_usage_example api_usage/api_usage_example.cpp)
# target_link_libraries(api_usage_example
#     PRIVATE
#         vkkk
#         OpenImageIO::OpenImageIO)
