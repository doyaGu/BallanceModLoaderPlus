cmake_minimum_required(VERSION 3.15)

# Public API headers for consumers of this module
add_library(BML_Console_api INTERFACE)
target_include_directories(BML_Console_api INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:modules/BML_Console/include>
)
bml_register_module_api(BML_Console_api)

bml_add_module(BML_Console
    SOURCES
        src/ConsoleMod.cpp
        src/BuiltinCommands.cpp
        src/AnsiPalette.cpp
        src/AnsiText.cpp
        src/AnsiSegments.cpp
    MANIFEST mod.toml
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/Utils
        ${VIRTOOLS_SDK_INCLUDE_DIR}
        ${IMGUI_SOURCE_DIR}
    LINK_LIBRARIES
        BML_Utils
)

add_custom_command(TARGET BML_Console PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/src/Utils/imconfig.h" "${IMGUI_SOURCE_DIR}"
    VERBATIM
)

install(TARGETS BML_Console
    RUNTIME DESTINATION Mods/BML_Console
    LIBRARY DESTINATION Mods/BML_Console
)

install(FILES mod.toml DESTINATION Mods/BML_Console)

install(DIRECTORY include/
    DESTINATION modules/BML_Console/include
    COMPONENT sdk
)
