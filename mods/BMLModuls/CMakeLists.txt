# BMLModuls
add_bml_mod(BMLModuls BMLModuls.cpp BMLModuls.h)

add_custom_command(TARGET BMLModuls POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:BMLModuls>" "${CMAKE_CURRENT_BINARY_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/3D Entities" "${CMAKE_CURRENT_BINARY_DIR}/3D Entities"
        COMMAND ${CMAKE_COMMAND} -E tar "cf" "BMLModuls.zip" --format=zip
        "$<TARGET_FILE_NAME:BMLModuls>" "3D Entities"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        VERBATIM
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/BMLModuls.zip" DESTINATION mods)
