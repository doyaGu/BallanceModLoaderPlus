
# Public API headers for consumers of this module
add_library(BML_Menu_api INTERFACE)
target_include_directories(BML_Menu_api INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:modules/BML_Menu/include>
)
bml_register_module_api(BML_Menu_api)

bml_add_module(BML_Menu
    SOURCES
        src/Menu.cpp
        src/MenuMod.cpp
    MANIFEST mod.toml
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${VIRTOOLS_SDK_INCLUDE_DIR}
        ${IMGUI_SOURCE_DIR}
    LINK_LIBRARIES
        VirtoolsSDK::CK2
        VirtoolsSDK::VxMath
)

add_custom_command(TARGET BML_Menu PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/modules/BML_UI/src/imconfig.h" "${IMGUI_SOURCE_DIR}"
    VERBATIM
)

install(TARGETS BML_Menu
    RUNTIME DESTINATION Mods/BML_Menu
    LIBRARY DESTINATION Mods/BML_Menu
)

install(FILES mod.toml DESTINATION Mods/BML_Menu)

install(DIRECTORY include/
    DESTINATION modules/BML_Menu/include
    COMPONENT sdk
)
