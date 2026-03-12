cmake_minimum_required(VERSION 3.15)

project(BML_HUD VERSION 0.4.0 LANGUAGES CXX)

set(BML_HUD_OUTPUT_DIR "${CMAKE_BINARY_DIR}/Mods/BML_HUD")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BML_HUD_SOURCES
    src/HudMod.cpp
    ${IMGUI_SOURCE_DIR}/imgui.cpp
    ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
    ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
    ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
)

add_library(BML_HUD SHARED ${BML_HUD_SOURCES})

target_include_directories(BML_HUD PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${IMGUI_SOURCE_DIR}
)

target_link_libraries(BML_HUD PRIVATE
    BMLUtils
)

target_compile_definitions(BML_HUD PRIVATE IMGUI_EXPORT)

set_target_properties(BML_HUD PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BML_HUD_OUTPUT_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${BML_HUD_OUTPUT_DIR}
)

add_custom_command(TARGET BML_HUD PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/modules/BML_UI/src/imconfig.h" "${IMGUI_SOURCE_DIR}"
    VERBATIM
)

add_custom_command(TARGET BML_HUD POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/mod.toml
        $<TARGET_FILE_DIR:BML_HUD>/mod.toml
)

install(TARGETS BML_HUD
    RUNTIME DESTINATION Mods/BML_HUD
    LIBRARY DESTINATION Mods/BML_HUD
)

install(FILES mod.toml DESTINATION Mods/BML_HUD)