# BallSticky
add_bml_mod(BallSticky BallSticky.cpp BallSticky.h)

add_custom_command(TARGET BallSticky POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:BallSticky>" "${CMAKE_CURRENT_BINARY_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/3D Entities" "${CMAKE_CURRENT_BINARY_DIR}/3D Entities"
        COMMAND ${CMAKE_COMMAND} -E tar "cf" "BallSticky.zip" --format=zip
        "$<TARGET_FILE_NAME:BallSticky>" "3D Entities"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        VERBATIM
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/BallSticky.zip" DESTINATION mods)
