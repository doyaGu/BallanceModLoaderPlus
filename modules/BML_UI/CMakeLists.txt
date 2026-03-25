cmake_minimum_required(VERSION 3.15)

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

# Public API headers for consumers of this module
add_library(BML_UI_api INTERFACE)
target_include_directories(BML_UI_api INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:modules/BML_UI/include>
)
bml_register_module_api(BML_UI_api)

bml_add_module(BML_UI
    SOURCES
        src/UIMod.cpp
        src/Win32Hooks.cpp
        src/imgui_impl_ck2.cpp
        ${CIMGUI_SOURCE_DIR}/cimgui.cpp
        src/UIMod.h
        src/Win32Hooks.h
        src/imgui_impl_ck2.h
        ${IMGUI_SOURCES}
        ${IMGUI_HEADERS}
    MANIFEST mod.toml
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CIMGUI_SOURCE_DIR}
        ${VIRTOOLS_SDK_INCLUDE_DIR}
        ${IMGUI_SOURCE_DIR}
    LINK_LIBRARIES
        VirtoolsSDK::CK2
        VirtoolsSDK::VxMath
        minhook
        BML_PathUtils
        BML_StringUtils
    COMPILE_DEFINITIONS
        BML_EXPORTS
        IMGUI_EXPORT
)

if (TARGET generate_bml_imgui_api)
    add_dependencies(BML_UI generate_bml_imgui_api)
endif ()

target_compile_options(BML_UI PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/FS>
)

add_custom_command(TARGET BML_UI PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/src/imconfig.h" "${IMGUI_SOURCE_DIR}"
    VERBATIM
)

install(TARGETS BML_UI
    RUNTIME DESTINATION Mods/BML_UI
    LIBRARY DESTINATION Mods/BML_UI
)

install(FILES mod.toml
    DESTINATION Mods/BML_UI
)

install(DIRECTORY include/
    DESTINATION modules/BML_UI/include
    COMPONENT sdk
)
