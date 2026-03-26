
bml_add_module(BML_DevTools
    SOURCES
        src/DevToolsMod.cpp
    MANIFEST mod.toml
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}
        ${IMGUI_SOURCE_DIR}
    LINK_LIBRARIES
        BML_UI
)

add_custom_command(TARGET BML_DevTools PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/modules/BML_UI/src/imconfig.h" "${IMGUI_SOURCE_DIR}"
    VERBATIM
)

bml_add_module_resources(BML_DevTools
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/locale
    DESTINATION locale
)

install(TARGETS BML_DevTools
    RUNTIME DESTINATION Mods/BML_DevTools
    LIBRARY DESTINATION Mods/BML_DevTools
)

install(FILES mod.toml DESTINATION Mods/BML_DevTools)
install(DIRECTORY locale DESTINATION Mods/BML_DevTools)
