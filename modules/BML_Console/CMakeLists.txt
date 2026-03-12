cmake_minimum_required(VERSION 3.15)

project(BML_Console VERSION 0.4.0 LANGUAGES CXX)

set(BML_CONSOLE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/Mods/BML_Console")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BML_CONSOLE_SOURCES
    src/ConsoleMod.cpp
    ${IMGUI_SOURCE_DIR}/imgui.cpp
    ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
    ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
    ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
)

add_library(BML_Console SHARED ${BML_CONSOLE_SOURCES})

target_include_directories(BML_Console PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${IMGUI_SOURCE_DIR}
)

target_link_libraries(BML_Console PRIVATE
    BMLUtils
)

target_compile_definitions(BML_Console PRIVATE IMGUI_EXPORT)

set_target_properties(BML_Console PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BML_CONSOLE_OUTPUT_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${BML_CONSOLE_OUTPUT_DIR}
)

add_custom_command(TARGET BML_Console PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/modules/BML_UI/src/imconfig.h" "${IMGUI_SOURCE_DIR}"
    VERBATIM
)

add_custom_command(TARGET BML_Console POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/mod.toml
        $<TARGET_FILE_DIR:BML_Console>/mod.toml
)

install(TARGETS BML_Console
    RUNTIME DESTINATION Mods/BML_Console
    LIBRARY DESTINATION Mods/BML_Console
)

install(FILES mod.toml DESTINATION Mods/BML_Console)