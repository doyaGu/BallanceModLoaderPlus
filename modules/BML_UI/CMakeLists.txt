cmake_minimum_required(VERSION 3.15)

project(BML_UI VERSION 0.4.0 LANGUAGES CXX)

set(BML_UI_OUTPUT_DIR "${CMAKE_BINARY_DIR}/Mods/BML_UI")

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ImGui sources
set(IMGUI_SOURCES
    ${IMGUI_SOURCE_DIR}/imgui.cpp
    ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
    ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
    ${IMGUI_SOURCE_DIR}/imgui_demo.cpp
    ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
    ${IMGUI_SOURCE_DIR}/backends/imgui_impl_win32.cpp
)

set(IMGUI_HEADERS
    ${IMGUI_SOURCE_DIR}/imgui.h
    ${IMGUI_SOURCE_DIR}/imgui_internal.h
    ${IMGUI_SOURCE_DIR}/imstb_rectpack.h
    ${IMGUI_SOURCE_DIR}/imstb_textedit.h
    ${IMGUI_SOURCE_DIR}/imstb_truetype.h
    ${IMGUI_SOURCE_DIR}/backends/imgui_impl_win32.h
)

# Module sources
set(BML_UI_SOURCES
    src/UIMod.cpp
    src/Overlay.cpp
    src/imgui_impl_ck2.cpp
    src/AnsiPalette.cpp
    src/AnsiText.cpp
)

set(BML_UI_HEADERS
    src/UIMod.h
    src/Overlay.h
    src/imgui_impl_ck2.h
    src/AnsiPalette.h
    src/AnsiText.h
)

# Create shared library (DLL)
add_library(BML_UI SHARED
    ${BML_UI_SOURCES}
    ${BML_UI_HEADERS}
    ${IMGUI_SOURCES}
    ${IMGUI_HEADERS}
)

# Include directories
target_include_directories(BML_UI PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src       # For shared headers
    ${VIRTOOLS_SDK_INCLUDE_DIR}
    ${IMGUI_SOURCE_DIR}
)

# Link dependencies
target_link_libraries(BML_UI PRIVATE
    VirtoolsSDK::CK2
    VirtoolsSDK::VxMath
    minhook
    BMLUtils    # For IniFile, PathUtils, StringUtils used by AnsiPalette
)

# Compile definitions
target_compile_definitions(BML_UI PRIVATE
    BML_UI_EXPORTS
    IMGUI_EXPORT
)

# Output configuration
set_target_properties(BML_UI PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BML_UI_OUTPUT_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${BML_UI_OUTPUT_DIR}
)

add_custom_command(TARGET BML_UI PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/src/imconfig.h" "${IMGUI_SOURCE_DIR}"
        VERBATIM
)

# Copy mod.toml to output directory
add_custom_command(TARGET BML_UI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/mod.toml
        $<TARGET_FILE_DIR:BML_UI>/mod.toml
)

install(TARGETS BML_UI
    RUNTIME DESTINATION Mods/BML_UI
    LIBRARY DESTINATION Mods/BML_UI
)

install(FILES mod.toml
    DESTINATION Mods/BML_UI
)
