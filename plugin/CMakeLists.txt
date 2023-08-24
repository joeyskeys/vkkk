file(ARCHIVE_CREATE
    OUTPUT "vulender.zip"
    PATHS
        "${CMAKE_CURRENT_SOURCE_DIR}/vulender"
    FORMAT "zip")

add_custom_target(create_zip
    DEPENDS "vulender.zip")