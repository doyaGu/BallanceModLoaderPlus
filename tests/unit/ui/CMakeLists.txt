add_bml_test(CursorVisibilityPolicyTest
        SOURCES CursorVisibilityPolicyTest.cpp
)

add_bml_test(ImeStateTest
        SOURCES
        ImeStateTest.cpp
        ${BML_SOURCE_DIR}/UI/Ime/NativePresentation.cpp
        ${BML_SOURCE_DIR}/UI/Ime/State.cpp
)

add_bml_test(ImePresentationTest
        SOURCES
        ImePresentationTest.cpp
        ${BML_SOURCE_DIR}/UI/Ime/RailLayout.cpp
)

add_bml_test(ImeOwnershipTest
        SOURCES ImeOwnershipTest.cpp
)

add_bml_test(FontRuntimeTest
        SOURCES
        FontRuntimeTest.cpp
        ${BML_SOURCE_DIR}/UI/FontRuntime.cpp
        ${IMGUI_SOURCE_DIR}/imgui.cpp
        ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
        ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
        ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
        DEPENDENCIES
        BMLUtils
)
target_include_directories(FontRuntimeTest BEFORE PRIVATE ${IMGUI_SOURCE_DIR})
target_compile_definitions(FontRuntimeTest PRIVATE
        IMGUI_EXPORT
        BML_TEST_FONT_DIRECTORY="${PROJECT_SOURCE_DIR}/packaging/runtime/ModLoader/Fonts"
)

add_bml_test(BuiMenuTest
        SOURCES
        BuiMenuTest.cpp
        ${BML_SOURCE_DIR}/UI/ImGuiStateRecovery.cpp
        ${IMGUI_SOURCE_DIR}/imgui.cpp
        ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
        ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
        ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
)
target_include_directories(BuiMenuTest BEFORE PRIVATE
        ${IMGUI_SOURCE_DIR}
        ${VIRTOOLS_SDK_INCLUDE_DIR}
)
target_compile_definitions(BuiMenuTest PRIVATE IMGUI_EXPORT)

add_bml_test(AnsiPaletteTest
        SOURCES
        AnsiPaletteTest.cpp
        ${BML_SOURCE_DIR}/UI/AnsiPalette.cpp
        DEPENDENCIES
        BMLUtils
)
# Need ImGui headers for AnsiPalette
target_include_directories(AnsiPaletteTest BEFORE PRIVATE ${IMGUI_SOURCE_DIR})

add_bml_test(GameFontCatalogTest
        SOURCES
        GameFontCatalogTest.cpp
        ${BML_SOURCE_DIR}/UI/GameFontCatalog.cpp
)

add_bml_test(LegacyTextFontTest
        SOURCES
        LegacyTextFontTest.cpp
        ${BML_SOURCE_DIR}/UI/Gui/LegacyTextFont.cpp
        DEPENDENCIES
        VirtoolsSDK::CK2 VirtoolsSDK::VxMath
)

add_bml_test(AnsiTextTest
        SOURCES
        AnsiTextTest.cpp
        ${BML_SOURCE_DIR}/UI/AnsiText.cpp
        ${BML_SOURCE_DIR}/UI/AnsiPalette.cpp
        ${IMGUI_SOURCE_DIR}/imgui.cpp
        ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
        ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
        ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
        DEPENDENCIES
        BMLUtils
)
target_include_directories(AnsiTextTest BEFORE PRIVATE ${IMGUI_SOURCE_DIR})
target_compile_definitions(AnsiTextTest PRIVATE IMGUI_EXPORT)

add_bml_test(ImGuiStateRecoveryTest
        SOURCES
        ImGuiStateRecoveryTest.cpp
        ${BML_SOURCE_DIR}/UI/ImGuiStateRecovery.cpp
        ${BML_SOURCE_DIR}/UI/ScriptImGui.cpp
        ${IMGUI_SOURCE_DIR}/imgui.cpp
        ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
        ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
        ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
)
target_include_directories(ImGuiStateRecoveryTest BEFORE PRIVATE ${IMGUI_SOURCE_DIR})
target_compile_definitions(ImGuiStateRecoveryTest PRIVATE IMGUI_EXPORT)
