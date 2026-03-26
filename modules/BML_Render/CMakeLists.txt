
bml_add_module(BML_Render
    SOURCES
        src/RenderMod.cpp
        src/RenderHook.cpp
        src/RenderHook.h
        src/Macros.h
    MANIFEST mod.toml
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${VIRTOOLS_SDK_INCLUDE_DIR}
    LINK_LIBRARIES
        BML_HookUtils
        VirtoolsSDK::CK2
        VirtoolsSDK::VxMath
        minhook
        BML_Event_api
)

install(TARGETS BML_Render
    RUNTIME DESTINATION Mods/BML_Render
    LIBRARY DESTINATION Mods/BML_Render
)

install(FILES mod.toml
    DESTINATION Mods/BML_Render
)
