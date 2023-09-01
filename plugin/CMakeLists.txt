add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/vulender.zip
    COMMAND ${CMAKE_COMMAND} -E tar cfv ${CMAKE_BINARY_DIR}/vulender.zip
        --format=zip ${CMAKE_CURRENT_SOURCE_DIR}/vulender
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/vulender)

add_custom_target(create_zip ALL
    DEPENDS "vulender.zip")