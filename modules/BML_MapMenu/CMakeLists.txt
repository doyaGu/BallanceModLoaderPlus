cmake_minimum_required(VERSION 3.15)

project(BML_MapMenu VERSION 0.4.0 LANGUAGES CXX)

set(BML_MAPMENU_OUTPUT_DIR "${CMAKE_BINARY_DIR}/Mods/BML_MapMenu")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BML_MAPMENU_SOURCES
    src/MapMenuMod.cpp
    ${IMGUI_SOURCE_DIR}/imgui.cpp
    ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
    ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
    ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
)

add_library(BML_MapMenu SHARED ${BML_MAPMENU_SOURCES})

find_package(VirtoolsSDK REQUIRED)

target_include_directories(BML_MapMenu PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/BML
    ${VIRTOOLS_SDK_INCLUDE_DIR}
    ${IMGUI_SOURCE_DIR}
)

target_link_libraries(BML_MapMenu PRIVATE
    VirtoolsSDK::CK2
    VirtoolsSDK::VxMath
)

target_compile_definitions(BML_MapMenu PRIVATE IMGUI_EXPORT)

set_target_properties(BML_MapMenu PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BML_MAPMENU_OUTPUT_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${BML_MAPMENU_OUTPUT_DIR}
)

add_custom_command(TARGET BML_MapMenu PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/modules/BML_UI/src/imconfig.h" "${IMGUI_SOURCE_DIR}"
    VERBATIM
)

add_custom_command(TARGET BML_MapMenu POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/mod.toml
        $<TARGET_FILE_DIR:BML_MapMenu>/mod.toml
)

install(TARGETS BML_MapMenu
    RUNTIME DESTINATION Mods/BML_MapMenu
    LIBRARY DESTINATION Mods/BML_MapMenu
)

install(FILES mod.toml DESTINATION Mods/BML_MapMenu)