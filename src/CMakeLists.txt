set(BML_VERSION_HEADER "${BML_INCLUDE_DIR}/BML/Version.h")
set(BML_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(BML_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(BML_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(BML_VERSION "${PROJECT_VERSION}")
configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/Version.h.in"
        "${BML_VERSION_HEADER}"
        @ONLY
)

set(BML_BUILD_VERSION_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(BML_BUILD_VERSION_HEADER "${BML_BUILD_VERSION_DIR}/BuildVersion.h")
set(BML_BUILD_VERSION_HEADER_ARGS
        -DBML_VERSION=${PROJECT_VERSION}
        -DTEMPLATE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/BuildVersion.h.in
        -DOUTPUT_FILE=${BML_BUILD_VERSION_HEADER}
        -DSOURCE_DIR=${PROJECT_SOURCE_DIR}
)

# Generate once during configure so the file exists before CMake validates the
# resource dependencies. The build target refreshes only this private header.
execute_process(
        COMMAND "${CMAKE_COMMAND}"
        ${BML_BUILD_VERSION_HEADER_ARGS}
        -P ${PROJECT_SOURCE_DIR}/cmake/GenerateBuildVersionHeader.cmake
        RESULT_VARIABLE BML_BUILD_VERSION_HEADER_RESULT
)
if (NOT BML_BUILD_VERSION_HEADER_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to generate ${BML_BUILD_VERSION_HEADER}")
endif ()

add_custom_target(BML_BuildVersionHeader ALL
    COMMAND "${CMAKE_COMMAND}"
        ${BML_BUILD_VERSION_HEADER_ARGS}
        -P ${PROJECT_SOURCE_DIR}/cmake/GenerateBuildVersionHeader.cmake
    BYPRODUCTS "${BML_BUILD_VERSION_HEADER}"
    COMMENT "Generating private build version"
)

# The IMC code generator is an authoring tool for mods that publish their own
# .imc interfaces. The loader ships none of its own, so nothing here runs it;
# the sample interface under tests/codegen/imc is what keeps the generator covered.
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)

add_subdirectory(Utils)

set(BML_PUBLIC_GUI_HEADERS
        ${BML_INCLUDE_DIR}/BML/Gui/Element.h
        ${BML_INCLUDE_DIR}/BML/Gui/Text.h
        ${BML_INCLUDE_DIR}/BML/Gui/Panel.h
        ${BML_INCLUDE_DIR}/BML/Gui/Label.h
        ${BML_INCLUDE_DIR}/BML/Gui/Button.h
        ${BML_INCLUDE_DIR}/BML/Gui/Input.h
        ${BML_INCLUDE_DIR}/BML/Gui/KeyInput.h
        ${BML_INCLUDE_DIR}/BML/Gui/Gui.h
)

set(BML_PUBLIC_GUID_HEADERS
        ${BML_INCLUDE_DIR}/BML/Guids/3DTransfo.h
        ${BML_INCLUDE_DIR}/BML/Guids/BuildingBlocksAddons1.h
        ${BML_INCLUDE_DIR}/BML/Guids/Cameras.h
        ${BML_INCLUDE_DIR}/BML/Guids/Collisions.h
        ${BML_INCLUDE_DIR}/BML/Guids/Controllers.h
        ${BML_INCLUDE_DIR}/BML/Guids/Grids.h
        ${BML_INCLUDE_DIR}/BML/Guids/Hooks.h
        ${BML_INCLUDE_DIR}/BML/Guids/Interface.h
        ${BML_INCLUDE_DIR}/BML/Guids/Lights.h
        ${BML_INCLUDE_DIR}/BML/Guids/Logics.h
        ${BML_INCLUDE_DIR}/BML/Guids/Materials.h
        ${BML_INCLUDE_DIR}/BML/Guids/MeshModifiers.h
        ${BML_INCLUDE_DIR}/BML/Guids/MidiManager.h
        ${BML_INCLUDE_DIR}/BML/Guids/Narratives.h
        ${BML_INCLUDE_DIR}/BML/Guids/physics_RT.h
        ${BML_INCLUDE_DIR}/BML/Guids/Sounds.h
        ${BML_INCLUDE_DIR}/BML/Guids/TT_DatabaseManager_RT.h
        ${BML_INCLUDE_DIR}/BML/Guids/TT_Gravity_RT.h
        ${BML_INCLUDE_DIR}/BML/Guids/TT_InterfaceManager_RT.h
        ${BML_INCLUDE_DIR}/BML/Guids/TT_ParticleSystems_RT.h
        ${BML_INCLUDE_DIR}/BML/Guids/TT_Toolbox_RT.h
        ${BML_INCLUDE_DIR}/BML/Guids/Visuals.h
        ${BML_INCLUDE_DIR}/BML/Guids/WorldEnvironments.h
)

set(BML_PUBLIC_HEADERS
        ${BML_VERSION_HEADER}
        ${BML_INCLUDE_DIR}/BML/Defines.h
        ${BML_INCLUDE_DIR}/BML/Result.hpp
        ${BML_INCLUDE_DIR}/BML/Interface.h
        ${BML_INCLUDE_DIR}/BML/Interface.hpp
        ${BML_INCLUDE_DIR}/BML/ModInterface.hpp
        ${BML_INCLUDE_DIR}/BML/BML.h
        ${BML_INCLUDE_DIR}/BML/Command.h
        ${BML_INCLUDE_DIR}/BML/Command.hpp

        ${BML_INCLUDE_DIR}/BML/ILogger.h
        ${BML_INCLUDE_DIR}/BML/ICommand.h
        ${BML_INCLUDE_DIR}/BML/IConfig.h
        ${BML_INCLUDE_DIR}/BML/IMessageReceiver.h
        ${BML_INCLUDE_DIR}/BML/IBML.h
        ${BML_INCLUDE_DIR}/BML/IMod.h
        ${BML_INCLUDE_DIR}/BML/DataShare.h
        ${BML_INCLUDE_DIR}/BML/DataShare.hpp
        ${BML_INCLUDE_DIR}/BML/Types.h
        ${BML_INCLUDE_DIR}/BML/Imc.h
        ${BML_INCLUDE_DIR}/BML/ImcWire.hpp
        ${BML_INCLUDE_DIR}/BML/Imc.hpp
        ${BML_INCLUDE_DIR}/BML/TypeConvert.h
        ${BML_INCLUDE_DIR}/BML/Behavior.h
        ${BML_PUBLIC_BEHAVIOR_CPP_HEADERS}
        ${BML_PUBLIC_BEHAVIOR_CPP_DETAIL_HEADERS}
        ${BML_PUBLIC_BEHAVIOR_BLOCK_HEADERS}
        ${BML_INCLUDE_DIR}/BML/Time.h
        ${BML_INCLUDE_DIR}/BML/Time.hpp
        ${BML_INCLUDE_DIR}/BML/Scene.h
        ${BML_INCLUDE_DIR}/BML/Gameplay.h
        ${BML_INCLUDE_DIR}/BML/UI.h
        ${BML_INCLUDE_DIR}/BML/ModMenu.h
        ${BML_INCLUDE_DIR}/BML/ModMenu.hpp
        ${BML_INCLUDE_DIR}/BML/Speedrun.h

        ${BML_INCLUDE_DIR}/BML/Bui.h
        ${BML_INCLUDE_DIR}/BML/Gui.h
        ${BML_PUBLIC_GUI_HEADERS}
        ${BML_INCLUDE_DIR}/BML/InputHook.h

        ${BML_INCLUDE_DIR}/BML/ExecuteBB.h
        ${BML_INCLUDE_DIR}/BML/ScriptHelper.h
        ${BML_INCLUDE_DIR}/BML/Guids.h
        ${BML_PUBLIC_GUID_HEADERS}

        ${BML_INCLUDE_DIR}/BML/BMLAll.h
)

set(BML_ROOT_SOURCES
        BML.cpp
)

set(BML_MODS_PRIVATE_HEADERS
        Mods/BMLCommand.h
        Mods/BMLConfigMigration.h
        Mods/BMLMod.h
        Mods/NewBallTypeMod.h
)

set(BML_MODS_SOURCES
        Mods/BMLCommand.cpp
        Mods/BMLConfigMigration.cpp
        Mods/BMLMod.cpp
        Mods/NewBallTypeMod.cpp
)

set(BML_LOADER_PRIVATE_HEADERS
        Loader/LegacyModVersion.h
        Loader/ModContext.h
        Loader/ModLoader.h
        Loader/ModInvocationGate.h
        Loader/ModManager.h
        Loader/NativeModRegistry.h
)

set(BML_LOADER_SOURCES
        Loader/ModContext.cpp
        Loader/ModLoader.cpp
        Loader/ModManager.cpp
        Loader/NativeModRegistry.cpp
)

set(BML_API_PRIVATE_HEADERS
        Api/BehaviorApi.h
        Api/BuiltinCapabilities.h
        Api/CommandApi.h
        Api/ExecuteBBAdapter.h
        Api/InterfaceRegistry.h
        Api/ObjectRefs.h
)

set(BML_API_SOURCES
        Api/BehaviorApi.cpp
        Api/BmlApi.cpp
        Api/BuiltinCapabilities.cpp
        Api/CommandApi.cpp
        Api/Errors.cpp
        Api/ExecuteBB.cpp
        Api/ExecuteBBAdapter.cpp
        Api/IMod.cpp
        Api/Interfaces.cpp
        Api/InterfaceRegistry.cpp
        Api/ObjectRefs.cpp
)

if (BML_ENABLE_BEHAVIOR_TEST_INTERFACE)
    list(APPEND BML_API_PRIVATE_HEADERS Api/BehaviorTestApi.h)
    list(APPEND BML_API_SOURCES Api/BehaviorTestApi.cpp)
endif ()

set(BML_DATA_SHARE_PRIVATE_HEADERS
        DataShare/DataShare.h
)

set(BML_DATA_SHARE_SOURCES
        DataShare/DataShare.cpp
)

set(BML_DIAGNOSTICS_PRIVATE_HEADERS
        Diagnostics/SystemReport.h
)

set(BML_DIAGNOSTICS_SOURCES
        Diagnostics/SystemReport.cpp
)

set(BML_LOGGING_PRIVATE_HEADERS
        Logging/Logger.h
)

set(BML_LOGGING_SOURCES
        Logging/Logger.cpp
)

set(BML_CONFIG_PRIVATE_HEADERS
        Config/Config.h
        Config/ConfigStore.h
        Config/ConfigValue.h
)

set(BML_CONFIG_SOURCES
        Config/Config.cpp
        Config/ConfigStore.cpp
)

set(BML_CONSOLE_PRIVATE_HEADERS
        Console/BuiltinCommands.h
        Console/CommandBar.h
        Console/CommandBarTheme.h
        Console/CommandContext.h
        Console/CommandInput.h
        Console/Console.h
        Console/ConsoleLayout.h
        Console/MessageBoard.h
        Console/Shell/FilterCommands.h
        Console/Shell/ShellBuiltins.h
        Console/Shell/ShellCompletion.h
        Console/Shell/ShellDispatcher.h
        Console/Shell/ShellEditing.h
        Console/Shell/ShellEnvironment.h
        Console/Shell/ShellExecutor.h
        Console/Shell/ShellExpander.h
        Console/Shell/ShellHighlighter.h
        Console/Shell/ShellHistory.h
        Console/Shell/ShellIo.h
        Console/Shell/ShellLexer.h
        Console/Shell/ShellParser.h
        Console/Shell/ShellQuoting.h
        Console/Shell/ShellTypes.h
)

set(BML_CONSOLE_SOURCES
        Console/BuiltinCommands.cpp
        Console/CommandBar.cpp
        Console/CommandBarTheme.cpp
        Console/CommandContext.cpp
        Console/CommandInput.cpp
        Console/Console.cpp
        Console/ConsoleLayout.cpp
        Console/MessageBoard.cpp
        Console/Shell/FilterCommands.cpp
        Console/Shell/ShellBuiltins.cpp
        Console/Shell/ShellCompletion.cpp
        Console/Shell/ShellEditing.cpp
        Console/Shell/ShellEnvironment.cpp
        Console/Shell/ShellExecutor.cpp
        Console/Shell/ShellExpander.cpp
        Console/Shell/ShellHighlighter.cpp
        Console/Shell/ShellHistory.cpp
        Console/Shell/ShellIo.cpp
        Console/Shell/ShellLexer.cpp
        Console/Shell/ShellParser.cpp
        Console/Shell/ShellQuoting.cpp
)

set(BML_HUD_PRIVATE_HEADERS
        HUD/FpsCounter.h
        HUD/HUD.h
        HUD/HUDCommand.h
        HUD/HUDRuntime.h
        HUD/RealTimer.h
        HUD/SRTimer.h
)

set(BML_HUD_SOURCES
        HUD/FpsCounter.cpp
        HUD/HUD.cpp
        HUD/HUDAnimation.cpp
        HUD/HUDCommand.cpp
        HUD/HUDRuntime.cpp
        HUD/RealTimer.cpp
        HUD/SRTimer.cpp
)

set(BML_CUSTOM_MAPS_PRIVATE_HEADERS
        CustomMaps/CustomMapLoad.h
        CustomMaps/CustomMaps.h
        CustomMaps/LevelLoader.h
        CustomMaps/MapCatalog.h
        CustomMaps/MapCommand.h
        CustomMaps/MapMenu.h
        CustomMaps/MapMenuState.h
        CustomMaps/MapStaging.h
)

set(BML_CUSTOM_MAPS_SOURCES
        CustomMaps/CustomMaps.cpp
        CustomMaps/LevelLoader.cpp
        CustomMaps/MapCatalog.cpp
        CustomMaps/MapCommand.cpp
        CustomMaps/MapMenu.cpp
        CustomMaps/MapMenuState.cpp
        CustomMaps/MapStaging.cpp
)

set(BML_GAMEPLAY_PRIVATE_HEADERS
        Gameplay/CheatCommand.h
        Gameplay/GameEventHooks.h
        Gameplay/GameplayTweaks.h
)

set(BML_GAMEPLAY_SOURCES
        Gameplay/CheatCommand.cpp
        Gameplay/GameEventHooks.cpp
        Gameplay/GameplayTweaks.cpp
)

set(BML_IMC_PRIVATE_HEADERS
        Imc/ImcPrimitives.h
        Imc/ImcRuntime.h
)

set(BML_IMC_SOURCES
        Imc/ImcApi.cpp
        Imc/ImcRuntime.cpp
)

set(BML_MOD_MENU_PRIVATE_HEADERS
        ModMenu/ModMenu.h
        ModMenu/ModsMenuEntry.h
        ModMenu/ModMenuPages.h
        ModMenu/ModMenuModel.h
        ModMenu/ModMenuPresentation.h
        ModMenu/ModMenuSession.h
)

set(BML_MOD_MENU_SOURCES
        ModMenu/ModMenu.cpp
        ModMenu/ModsMenuEntry.cpp
        ModMenu/ModMenuPages.cpp
        ModMenu/ModMenuModel.cpp
        ModMenu/ModMenuPresentation.cpp
        ModMenu/ModMenuSession.cpp
)

set(BML_UI_PRIVATE_HEADERS
        UI/AnsiPalette.h
        UI/AnsiText.h
        UI/BuiInternal.h
        UI/ColorCodec.h
        UI/FontCommand.h
        UI/Ime/Presentation.h
        UI/Ime/NativePresentation.h
        UI/Ime/PresentationOwnership.h
        UI/Ime/RailLayout.h
        UI/Ime/Runtime.h
        UI/Ime/State.h
        UI/Ime/Tsf.h
        UI/FontRuntime.h
        UI/GameFontCatalog.h
        UI/Gui/LegacyTextFont.h
        UI/ImGuiStateRecovery.h
        UI/Overlay.h
        UI/OverlayPlatformInput.h
        UI/PaletteCommand.h
        UI/ScriptImGui.h
)

set(BML_UI_SOURCES
        UI/AnsiPalette.cpp
        UI/AnsiText.cpp
        UI/Bui.cpp
        UI/ColorCodec.cpp
        UI/FontCommand.cpp
        UI/Ime/Presentation.cpp
        UI/Ime/NativePresentation.cpp
        UI/Ime/RailLayout.cpp
        UI/Ime/Runtime.cpp
        UI/Ime/State.cpp
        UI/Ime/Tsf.cpp
        UI/FontRuntime.cpp
        UI/GameFontCatalog.cpp
        UI/ImGuiStateRecovery.cpp
        UI/Overlay.cpp
        UI/OverlayPlatformInput.cpp
        UI/PaletteCommand.cpp
        UI/ScriptImGui.cpp
        UI/Gui/Button.cpp
        UI/Gui/Element.cpp
        UI/Gui/Gui.cpp
        UI/Gui/Input.cpp
        UI/Gui/KeyInput.cpp
        UI/Gui/Label.cpp
        UI/Gui/LegacyTextFont.cpp
        UI/Gui/Panel.cpp
        UI/Gui/Text.cpp
)

set(BML_HOOK_PRIVATE_HEADERS
        Hooks/BehaviorFunctionPatch.h
        Hooks/CKRasterizer.h
        Hooks/CKRasterizerEnums.h
        Hooks/CKRasterizerTypes.h
        Hooks/HookLifecycle.h
        Hooks/InputCursor.h
        Hooks/Macros.h
        Hooks/RenderHook.h
        Hooks/VTablePatch.h
        Hooks/VTables.h
)

set(BML_HOOK_SOURCES
        Hooks/BehaviorFunctionPatch.cpp
        Hooks/HookLifecycle.cpp
        Hooks/InputHook.cpp
        Hooks/ObjectLoadHook.cpp
        Hooks/PhysicsHook.cpp
        Hooks/RenderHook.cpp
        Hooks/VTablePatch.cpp
)

set(BML_BEHAVIOR_PRIVATE_HEADERS
        Behavior/Block.h
        Behavior/Callback.h
        Behavior/CKBehaviorContext.h
        Behavior/CKEdit.h
        Behavior/CKGraphOrder.h
        Behavior/Edit.h
        Behavior/Execution.h
        Behavior/FrameStore.h
        Behavior/Graph.h
        Behavior/GraphEdit.h
        Behavior/Lifecycle.h
        Behavior/Layout.h
        Behavior/ObjectRef.h
        Behavior/Parameter.h
        Behavior/Patches.h
        Behavior/Pattern.h
        Behavior/Plan.h
        Behavior/PrototypeCatalog.h
        Behavior/Relations.h
        Behavior/Runtime.h
        Behavior/Script.h
        Behavior/Sessions.h
        Behavior/Status.h
        Behavior/Topology.h
        Behavior/Value.h
        Behavior/Watch.h

        Behavior/Blocks/HookBlock.h
        Behavior/Blocks/PhysicsForce.h
        Behavior/Blocks/Text2DView.h
)

set(BML_BEHAVIOR_SOURCES
        Behavior/Block.cpp
        Behavior/Callback.cpp
        Behavior/CKEdit.cpp
        Behavior/CKGraph.cpp
        Behavior/CKGraphOrder.cpp
        Behavior/CKPrototypeSource.cpp
        Behavior/Edit.cpp
        Behavior/Execution.cpp
        Behavior/FrameStore.cpp
        Behavior/GraphEdit.cpp
        Behavior/Lifecycle.cpp
        Behavior/Layout.cpp
        Behavior/Parameter.cpp
        Behavior/Patches.cpp
        Behavior/Pattern.cpp
        Behavior/Plan.cpp
        Behavior/PrototypeCatalog.cpp
        Behavior/Relations.cpp
        Behavior/Runtime.cpp
        Behavior/Script.cpp
        Behavior/ScriptHelper.cpp
        Behavior/Sessions.cpp
        Behavior/Topology.cpp
        Behavior/Value.cpp
        Behavior/Watch.cpp

        Behavior/Blocks/Hook.cpp
        Behavior/Blocks/HookBlock.cpp
        Behavior/Blocks/PhysicsForce.cpp
        Behavior/Blocks/Text2DView.cpp
)

set(BML_PRIVATE_HEADERS
        ${BML_MODS_PRIVATE_HEADERS}
        ${BML_LOADER_PRIVATE_HEADERS}
        ${BML_API_PRIVATE_HEADERS}
        ${BML_DATA_SHARE_PRIVATE_HEADERS}
        ${BML_DIAGNOSTICS_PRIVATE_HEADERS}
        ${BML_LOGGING_PRIVATE_HEADERS}
        ${BML_CONFIG_PRIVATE_HEADERS}
        ${BML_CONSOLE_PRIVATE_HEADERS}
        ${BML_HUD_PRIVATE_HEADERS}
        ${BML_CUSTOM_MAPS_PRIVATE_HEADERS}
        ${BML_GAMEPLAY_PRIVATE_HEADERS}
        ${BML_IMC_PRIVATE_HEADERS}
        ${BML_MOD_MENU_PRIVATE_HEADERS}
        ${BML_UI_PRIVATE_HEADERS}
        ${BML_HOOK_PRIVATE_HEADERS}
        ${BML_BEHAVIOR_PRIVATE_HEADERS}
)

set(BML_SOURCES
        ${BML_ROOT_SOURCES}
        ${BML_MODS_SOURCES}
        ${BML_LOADER_SOURCES}
        ${BML_API_SOURCES}
        ${BML_DATA_SHARE_SOURCES}
        ${BML_DIAGNOSTICS_SOURCES}
        ${BML_LOGGING_SOURCES}
        ${BML_CONFIG_SOURCES}
        ${BML_CONSOLE_SOURCES}
        ${BML_HUD_SOURCES}
        ${BML_CUSTOM_MAPS_SOURCES}
        ${BML_GAMEPLAY_SOURCES}
        ${BML_IMC_SOURCES}
        ${BML_MOD_MENU_SOURCES}
        ${BML_UI_SOURCES}
        ${BML_HOOK_SOURCES}
        ${BML_BEHAVIOR_SOURCES}
        BML.rc
)

set_source_files_properties(BML.rc PROPERTIES
        OBJECT_DEPENDS "${BML_BUILD_VERSION_HEADER}"
)

# Keep the script command in both builds so the disabled variant can report why it is unavailable.
set(BML_ANGELSCRIPT_PRIVATE_HEADERS
        AngelScript/ScriptCommand.h
)
set(BML_ANGELSCRIPT_SOURCES
        AngelScript/ScriptCommand.cpp
)
if (BML_WITH_ANGELSCRIPT)
    list(APPEND BML_ANGELSCRIPT_PRIVATE_HEADERS
            AngelScript/CKAngelScriptAdapter.h
            AngelScript/AngelScriptBindings.h
            AngelScript/AngelScriptImGuiBindings.h
            AngelScript/ScriptApiSurface.h
            AngelScript/ScriptBehavior.h
            AngelScript/ScriptBehaviorBindings.inc
            AngelScript/ScriptAngelScriptHandle.h
            AngelScript/ScriptAvailabilityLogLimiter.h
            AngelScript/ScriptCallbackDispatcher.h
            AngelScript/ScriptCallbackEvents.h
            AngelScript/ScriptCommandService.h
            AngelScript/ScriptDataShareService.h
            AngelScript/ScriptDevEvents.h
            AngelScript/ScriptDevToolsService.h
            AngelScript/ScriptDiagnostic.h
            AngelScript/ScriptFacadeAccess.h
            AngelScript/ScriptFileWatcherWin32.h
            AngelScript/ScriptHookBlockService.h
            AngelScript/ScriptImcRecord.h
            AngelScript/ScriptImcService.h
            AngelScript/ScriptLibraryRegistry.h
            AngelScript/ScriptLibraryServices.h
            AngelScript/ScriptLibraryTools.h
            AngelScript/ScriptLibraryValidator.h
            AngelScript/ScriptMenuPageService.h
            AngelScript/ScriptMod.h
            AngelScript/ScriptModContextView.h
            AngelScript/ScriptBuiltinFacade.h
            AngelScript/ScriptModDefinition.h
            AngelScript/ScriptModDefinitionBuilder.h
            AngelScript/ScriptModEntryScanner.h
            AngelScript/ScriptModHotReloadPathFilter.h
            AngelScript/ScriptModHotReloadService.h
            AngelScript/ScriptModLifecycle.h
            AngelScript/ScriptModReloadCandidateInternal.h
            AngelScript/ScriptModLoader.h
            AngelScript/ScriptModReloadTransaction.h
            AngelScript/ScriptModRuntime.h
            AngelScript/ScriptModState.h
            AngelScript/ScriptReloadCandidateBuilder.h
            AngelScript/ScriptReloadStateValidator.h
            AngelScript/ScriptReloadStagingCleanup.h
            AngelScript/ScriptSourceSnapshot.h
            AngelScript/ScriptSourceSnapshotBuilder.h
            AngelScript/ScriptStringInterop.h
            AngelScript/ScriptStateBag.h
            AngelScript/ScriptStateMigration.h
            AngelScript/ScriptTimerService.h
            AngelScript/generated/BMLImGuiAngelScriptBindings.h
    )
    list(APPEND BML_ANGELSCRIPT_SOURCES
            AngelScript/AngelScriptBindings.cpp
            AngelScript/CKAngelScriptAdapter.cpp
            AngelScript/ScriptApiSurface.cpp
            AngelScript/ScriptBehavior.cpp
            AngelScript/ScriptCallbackDispatcher.cpp
            AngelScript/ScriptCallbackEvents.cpp
            AngelScript/ScriptCommandService.cpp
            AngelScript/ScriptDataShareService.cpp
            AngelScript/ScriptDevEvents.cpp
            AngelScript/ScriptDevToolsService.cpp
            AngelScript/ScriptDiagnostic.cpp
            AngelScript/ScriptFileWatcherWin32.cpp
            AngelScript/ScriptFunctionSupport.cpp
            AngelScript/ScriptHookBlockService.cpp
            AngelScript/ScriptImcRecord.cpp
            AngelScript/ScriptImcService.cpp
            AngelScript/ScriptLibraryRegistry.cpp
            AngelScript/ScriptLibraryServices.cpp
            AngelScript/ScriptLibraryTools.cpp
            AngelScript/ScriptLibraryValidator.cpp
            AngelScript/ScriptMenuPageService.cpp
            AngelScript/ScriptMod.cpp
            AngelScript/ScriptModContextView.cpp
            AngelScript/ScriptBuiltinFacade.cpp
            AngelScript/ScriptModDefinition.cpp
            AngelScript/ScriptModDefinitionBuilder.cpp
            AngelScript/ScriptModEntryScanner.cpp
            AngelScript/ScriptModHotReloadPathFilter.cpp
            AngelScript/ScriptModHotReloadService.cpp
            AngelScript/ScriptModLifecycle.cpp
            AngelScript/ScriptModLoader.cpp
            AngelScript/ScriptModReloadCandidateInternal.cpp
            AngelScript/ScriptModReloadTransaction.cpp
            AngelScript/ScriptModRuntime.cpp
            AngelScript/ScriptModState.cpp
            AngelScript/ScriptReloadCandidateBuilder.cpp
            AngelScript/ScriptReloadStateValidator.cpp
            AngelScript/ScriptReloadStagingCleanup.cpp
            AngelScript/ScriptSourceSnapshotBuilder.cpp
            AngelScript/ScriptStringInterop.cpp
            AngelScript/ScriptStateBag.cpp
            AngelScript/ScriptStateMigration.cpp
            AngelScript/ScriptTimerService.cpp
            AngelScript/AngelScriptImGuiBindings.cpp
            AngelScript/generated/BMLImGuiAngelScriptBindings.cpp
    )
endif ()

set(IMGUI_HEADERS
        ${IMGUI_SOURCE_DIR}/imgui.h
        ${IMGUI_SOURCE_DIR}/imgui_internal.h
        ${IMGUI_SOURCE_DIR}/imstb_rectpack.h
        ${IMGUI_SOURCE_DIR}/imstb_textedit.h
        ${IMGUI_SOURCE_DIR}/imstb_truetype.h
        ${IMGUI_SOURCE_DIR}/backends/imgui_impl_win32.h
        ${IMGUI_SOURCE_DIR}/misc/cpp/imgui_stdlib.h
        ${CMAKE_CURRENT_SOURCE_DIR}/UI/imgui_impl_ck2.h
)

set(IMGUI_SOURCES
        ${IMGUI_SOURCE_DIR}/imgui.cpp
        ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
        ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
        ${IMGUI_SOURCE_DIR}/imgui_demo.cpp
        ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
        ${IMGUI_SOURCE_DIR}/backends/imgui_impl_win32.cpp
        ${IMGUI_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/UI/imgui_impl_ck2.cpp
)

add_library(BML SHARED
        ${BML_SOURCES} ${BML_PUBLIC_HEADERS} ${BML_PRIVATE_HEADERS}
        ${BML_ANGELSCRIPT_PRIVATE_HEADERS}
        ${BML_ANGELSCRIPT_SOURCES}
        ${IMGUI_SOURCES} ${IMGUI_HEADERS}
        ${BML_BUILD_VERSION_HEADER}
)

if (BML_BUILD_UI_AUTOMATION)
    include("${PROJECT_SOURCE_DIR}/tests/ui/PlayerSources.cmake")
endif ()

target_compile_features(BML PUBLIC cxx_std_20)

target_compile_options(BML PRIVATE
        $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/MP>
)

target_include_directories(BML
        PUBLIC
        $<BUILD_INTERFACE:${BML_INCLUDE_DIR}>
        $<BUILD_INTERFACE:${IMGUI_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${BML_BUILD_VERSION_DIR}
)

if (BML_WITH_ANGELSCRIPT)
    target_include_directories(BML PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/AngelScript
            ${CKANGELSCRIPT_INCLUDE_DIR}
            ${CKANGELSCRIPT_ANGELSCRIPT_INCLUDE_DIR}
    )
endif ()

target_link_libraries(BML
        PUBLIC
        VirtoolsSDK::CK2 VirtoolsSDK::VxMath
        PRIVATE
        BMLUtils
        minhook onig imm32 comctl32 ole32 oleaut32 uuid
)

add_dependencies(BML BML_BuildVersionHeader)

target_compile_definitions(BML
        PUBLIC
        "$<BUILD_INTERFACE:IMGUI_USER_CONFIG=\"${CMAKE_CURRENT_SOURCE_DIR}/BMLImGuiConfig.h\">"
        "$<INSTALL_INTERFACE:IMGUI_USER_CONFIG=\"BMLImGuiConfig.h\">"
        PRIVATE
        BML_EXPORTS IMGUI_EXPORT
        _WIN32_WINNT=0x0A00 WINVER=0x0A00
        BML_ENABLE_ANGELSCRIPT=$<BOOL:${BML_WITH_ANGELSCRIPT}>
        BML_ENABLE_BEHAVIOR_TEST_INTERFACE=$<BOOL:${BML_ENABLE_BEHAVIOR_TEST_INTERFACE}>
        BML_ENABLE_UI_AUTOMATION=$<BOOL:${BML_BUILD_UI_AUTOMATION}>
)

set_target_properties(BML PROPERTIES
        OUTPUT_NAME "BMLPlus"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

install(TARGETS BML
        EXPORT BMLTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

foreach (BML_PUBLIC_HEADER IN LISTS BML_PUBLIC_HEADERS)
    file(RELATIVE_PATH BML_PUBLIC_HEADER_RELATIVE "${BML_INCLUDE_DIR}" "${BML_PUBLIC_HEADER}")
    get_filename_component(BML_PUBLIC_HEADER_DIRECTORY "${BML_PUBLIC_HEADER_RELATIVE}" DIRECTORY)
    install(FILES "${BML_PUBLIC_HEADER}"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${BML_PUBLIC_HEADER_DIRECTORY}"
    )
endforeach ()

install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/imconfig.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/BMLImGuiConfig.h"
        "${IMGUI_SOURCE_DIR}/imgui.h"
        "${IMGUI_SOURCE_DIR}/imgui_internal.h"
        "${IMGUI_SOURCE_DIR}/imstb_rectpack.h"
        "${IMGUI_SOURCE_DIR}/imstb_textedit.h"
        "${IMGUI_SOURCE_DIR}/imstb_truetype.h"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT BMLTargets
        NAMESPACE BML::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/BML"
)

configure_package_config_file(
        "${PROJECT_SOURCE_DIR}/cmake/BMLConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/BMLConfig.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/BML"
)

write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/BMLConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMinorVersion
)

install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/BMLConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/BMLConfigVersion.cmake"
        "${PROJECT_SOURCE_DIR}/cmake/BMLMod.cmake"
        "${PROJECT_SOURCE_DIR}/cmake/BMLInterface.cmake"
        "${PROJECT_SOURCE_DIR}/cmake/BMLInterfacePackageConfig.cmake.in"
        "${PROJECT_SOURCE_DIR}/cmake/BMLImc.cmake"
        "${PROJECT_SOURCE_DIR}/cmake/FindVirtoolsSDK.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/BML"
)

install(FILES
        "${PROJECT_SOURCE_DIR}/tools/imc_codegen.py"
        "${PROJECT_SOURCE_DIR}/tools/interface_codegen.py"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/BML/tools"
)

install(FILES
        "${PROJECT_SOURCE_DIR}/packaging/sdk/README.md"
        "${PROJECT_SOURCE_DIR}/packaging/sdk/README_zh-CN.md"
        DESTINATION "."
)

install(FILES
        "${PROJECT_SOURCE_DIR}/templates/README.md"
        DESTINATION "templates"
)

install(DIRECTORY
        "${PROJECT_SOURCE_DIR}/templates/native-mod-template"
        "${PROJECT_SOURCE_DIR}/templates/native-interface-provider-template"
        "${PROJECT_SOURCE_DIR}/templates/native-interface-consumer-template"
        "${PROJECT_SOURCE_DIR}/templates/native-imc-provider-template"
        DESTINATION "templates"
)

install(DIRECTORY
        "${PROJECT_SOURCE_DIR}/examples/native-interface-provider"
        "${PROJECT_SOURCE_DIR}/examples/native-interface-consumer"
        DESTINATION "examples"
)

install(FILES
        "${PROJECT_SOURCE_DIR}/scripts/bml.cmd"
        "${PROJECT_SOURCE_DIR}/scripts/bml.py"
        DESTINATION "scripts"
)

install(FILES
        "${PROJECT_SOURCE_DIR}/scripts/lib/BMLProject.psm1"
        DESTINATION "scripts/lib"
)

install(FILES
        "${PROJECT_SOURCE_DIR}/docs/en/modding.md"
        "${PROJECT_SOURCE_DIR}/docs/en/native-mod-api.md"
        "${PROJECT_SOURCE_DIR}/docs/en/native-api-routes.md"
        "${PROJECT_SOURCE_DIR}/docs/en/behavior-authoring.md"
        "${PROJECT_SOURCE_DIR}/docs/en/imc-author-guide.md"
        "${PROJECT_SOURCE_DIR}/docs/en/imc.md"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/BML/docs/en"
)

install(FILES
        "${PROJECT_SOURCE_DIR}/docs/zh-CN/modding.md"
        "${PROJECT_SOURCE_DIR}/docs/zh-CN/native-mod-api.md"
        "${PROJECT_SOURCE_DIR}/docs/zh-CN/native-api-routes.md"
        "${PROJECT_SOURCE_DIR}/docs/zh-CN/behavior-authoring.md"
        "${PROJECT_SOURCE_DIR}/docs/zh-CN/imc-author-guide.md"
        "${PROJECT_SOURCE_DIR}/docs/zh-CN/imc.md"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/BML/docs/zh-CN"
)

if (BML_WITH_ANGELSCRIPT)
    install(FILES
            "${PROJECT_SOURCE_DIR}/docs/api/as.predefined"
            "${PROJECT_SOURCE_DIR}/docs/api/bml-script-mod-api.as"
            "${PROJECT_SOURCE_DIR}/docs/api/bml-imgui-api.as"
            DESTINATION "docs/api"
    )

    install(DIRECTORY
            "${PROJECT_SOURCE_DIR}/docs/en/script-mod"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/BML/docs/en"
            FILES_MATCHING PATTERN "*.md"
    )

    install(FILES
            "${PROJECT_SOURCE_DIR}/docs/zh-CN/api.md"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/BML/docs/zh-CN"
    )

    install(DIRECTORY
            "${PROJECT_SOURCE_DIR}/docs/zh-CN/script-mod-tutorial"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/BML/docs/zh-CN"
            FILES_MATCHING PATTERN "*.md"
    )

    install(DIRECTORY
            "${PROJECT_SOURCE_DIR}/examples/script-mod"
            DESTINATION "examples"
            FILES_MATCHING PATTERN "*.md" PATTERN "*.as"
    )

    install(DIRECTORY
            "${PROJECT_SOURCE_DIR}/templates/script-mod-template"
            DESTINATION "templates"
    )

endif ()
