cmake_minimum_required(VERSION 3.15)

bml_add_module(BML_ModMenu
    SOURCES
        src/Config.cpp
        src/MenuRegistry.cpp
        src/MenuRuntime.cpp
        src/ModMenu.cpp
        src/ModMenuMod.cpp
    MANIFEST mod.toml
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}
        ${VIRTOOLS_SDK_INCLUDE_DIR}
        ${IMGUI_SOURCE_DIR}
    LINK_LIBRARIES
        BML_Utils
        VirtoolsSDK::CK2
        VirtoolsSDK::VxMath
)

add_custom_command(TARGET BML_ModMenu PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/src/Utils/imconfig.h" "${IMGUI_SOURCE_DIR}"
    VERBATIM
)

install(TARGETS BML_ModMenu
    RUNTIME DESTINATION Mods/BML_ModMenu
    LIBRARY DESTINATION Mods/BML_ModMenu
)

install(FILES mod.toml DESTINATION Mods/BML_ModMenu)
