
bml_add_module(BML_Render
    SOURCES
        src/RenderMod.cpp
        src/RenderHook.h
        src/RenderHook.cpp
        src/CKRenderContextInternal.h
        src/CKRasterizer.h
        src/CKRasterizerEnums.h
        src/CKRasterizerTypes.h
    MANIFEST mod.toml
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${VIRTOOLS_SDK_INCLUDE_DIR}
    LINK_LIBRARIES
        BML_HookUtils
        VirtoolsSDK::CK2
        VirtoolsSDK::VxMath
        minhook
        BML_GameEvent_api
)

install(TARGETS BML_Render
    RUNTIME DESTINATION Mods/BML_Render
    LIBRARY DESTINATION Mods/BML_Render
)

install(FILES mod.toml
    DESTINATION Mods/BML_Render
)
