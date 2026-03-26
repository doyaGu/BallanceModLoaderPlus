bml_add_module(BML_MapMenu
    SOURCES
        src/MapMenuMod.cpp
    MANIFEST mod.toml
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${VIRTOOLS_SDK_INCLUDE_DIR}
        ${IMGUI_SOURCE_DIR}
    LINK_LIBRARIES
        VirtoolsSDK::CK2
        VirtoolsSDK::VxMath
        BML_PathUtils
        BML_StringUtils
)

add_custom_command(TARGET BML_MapMenu PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/modules/BML_UI/src/imconfig.h" "${IMGUI_SOURCE_DIR}"
    VERBATIM
)

bml_add_module_resources(BML_MapMenu
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/locale
    DESTINATION locale
)

install(TARGETS BML_MapMenu
    RUNTIME DESTINATION Mods/BML_MapMenu
    LIBRARY DESTINATION Mods/BML_MapMenu
)

install(FILES mod.toml DESTINATION Mods/BML_MapMenu)
install(DIRECTORY locale DESTINATION Mods/BML_MapMenu)
