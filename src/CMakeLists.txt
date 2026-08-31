set(BML_VERSION_HEADER "${BML_INCLUDE_DIR}/BML/Version.h")
set(BML_VERSION_HEADER_ARGS
        -DBML_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        -DBML_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        -DBML_VERSION_PATCH=${PROJECT_VERSION_PATCH}
        -DTEMPLATE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/Version.h.in
        -DOUTPUT_FILE=${BML_VERSION_HEADER}
        -DSOURCE_DIR=${PROJECT_SOURCE_DIR}
)

# Generate once during configure so the file exists before CMake validates target sources.
execute_process(
        COMMAND "${CMAKE_COMMAND}"
        ${BML_VERSION_HEADER_ARGS}
        -P ${PROJECT_SOURCE_DIR}/cmake/GenerateVersionHeader.cmake
        RESULT_VARIABLE BML_VERSION_HEADER_RESULT
)
if (NOT BML_VERSION_HEADER_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to generate ${BML_VERSION_HEADER}")
endif ()

add_custom_target(BML_VersionHeader ALL
    COMMAND "${CMAKE_COMMAND}"
        ${BML_VERSION_HEADER_ARGS}
        -P ${PROJECT_SOURCE_DIR}/cmake/GenerateVersionHeader.cmake
    BYPRODUCTS "${BML_VERSION_HEADER}"
    COMMENT "Generating Version.h with git metadata"
)

# The IMC code generator is an authoring tool for mods that publish their own
# .imc interfaces. The loader ships none of its own, so nothing here runs it;
# the sample interface under tests/imc is what keeps the generator covered.
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
        ${BML_INCLUDE_DIR}/BML/Interface.h
        ${BML_INCLUDE_DIR}/BML/BML.h

        ${BML_INCLUDE_DIR}/BML/ILogger.h
        ${BML_INCLUDE_DIR}/BML/ICommand.h
        ${BML_INCLUDE_DIR}/BML/IConfig.h
        ${BML_INCLUDE_DIR}/BML/IMessageReceiver.h
        ${BML_INCLUDE_DIR}/BML/IBML.h
        ${BML_INCLUDE_DIR}/BML/IMod.h
        ${BML_INCLUDE_DIR}/BML/DataShare.h
        ${BML_INCLUDE_DIR}/BML/Imc.h
        ${BML_INCLUDE_DIR}/BML/Types.h
        ${BML_INCLUDE_DIR}/BML/ImcWire.hpp
        ${BML_INCLUDE_DIR}/BML/ImcCpp.hpp
        ${BML_INCLUDE_DIR}/BML/TypeConvert.h
        ${BML_INCLUDE_DIR}/BML/Runtime.h
        ${BML_INCLUDE_DIR}/BML/Scene.h
        ${BML_INCLUDE_DIR}/BML/Gameplay.h
        ${BML_INCLUDE_DIR}/BML/UI.h
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
        Mods/BMLMod.h
        Mods/NewBallTypeMod.h
)

set(BML_MODS_SOURCES
        Mods/BMLMod.cpp
        Mods/NewBallTypeMod.cpp
)

set(BML_LOADER_PRIVATE_HEADERS
        Loader/LegacyModVersion.h
        Loader/ModContext.h
        Loader/ModInvocationGate.h
        Loader/ModManager.h
        Loader/NativeModRegistry.h
)

set(BML_LOADER_SOURCES
        Loader/ModContext.cpp
        Loader/ModManager.cpp
        Loader/NativeModRegistry.cpp
)

set(BML_API_PRIVATE_HEADERS
        Api/BuiltinCapabilities.h
        Api/ExecuteBBAdapter.h
        Api/ObjectRefs.h
)

set(BML_API_SOURCES
        Api/BmlApi.cpp
        Api/BuiltinCapabilities.cpp
        Api/Errors.cpp
        Api/ExecuteBB.cpp
        Api/ExecuteBBAdapter.cpp
        Api/IMod.cpp
        Api/Interfaces.cpp
        Api/ObjectRefs.cpp
)

set(BML_DATA_SHARE_PRIVATE_HEADERS
        DataShare/DataShare.hpp
)

set(BML_DATA_SHARE_SOURCES
        DataShare/DataShare.cpp
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
)

set(BML_CONFIG_SOURCES
        Config/Config.cpp
        Config/ConfigStore.cpp
)

set(BML_CONSOLE_PRIVATE_HEADERS
        Console/CommandBar.h
        Console/CommandContext.h
        Console/Commands.h
        Console/Console.h
        Console/MessageBoard.h
)

set(BML_CONSOLE_SOURCES
        Console/CommandBar.cpp
        Console/CommandContext.cpp
        Console/Commands.cpp
        Console/Console.cpp
        Console/MessageBoard.cpp
)

set(BML_HUD_PRIVATE_HEADERS
        HUD/FpsCounter.h
        HUD/HUD.h
        HUD/HUDRuntime.h
        HUD/RealTimer.h
        HUD/SRTimer.h
)

set(BML_HUD_SOURCES
        HUD/FpsCounter.cpp
        HUD/HUD.cpp
        HUD/HUDAnimation.cpp
        HUD/HUDRuntime.cpp
        HUD/RealTimer.cpp
        HUD/SRTimer.cpp
)

set(BML_CUSTOM_MAPS_PRIVATE_HEADERS
        CustomMaps/CustomMaps.h
        CustomMaps/MapCatalog.h
        CustomMaps/MapMenu.h
)

set(BML_CUSTOM_MAPS_SOURCES
        CustomMaps/CustomMaps.cpp
        CustomMaps/MapCatalog.cpp
        CustomMaps/MapMenu.cpp
)

set(BML_GAMEPLAY_PRIVATE_HEADERS
        Gameplay/GameEventHooks.h
        Gameplay/GameplayTweaks.h
        Gameplay/GameSession.h
)

set(BML_GAMEPLAY_SOURCES
        Gameplay/GameEventHooks.cpp
        Gameplay/GameplayTweaks.cpp
        Gameplay/GameSession.cpp
)

set(BML_IMC_PRIVATE_HEADERS
        Imc/ImcPrimitives.h
        Imc/ImcRuntime.h
)

set(BML_IMC_SOURCES
        Imc/ImcApi.cpp
        Imc/ImcRuntime.cpp
)

set(BML_UI_PRIVATE_HEADERS
        UI/AnsiPalette.h
        UI/AnsiText.h
        UI/BuiInternal.h
        UI/GameFontCatalog.h
        UI/ModMenu.h
        UI/Overlay.h
)

set(BML_UI_SOURCES
        UI/AnsiPalette.cpp
        UI/AnsiText.cpp
        UI/Bui.cpp
        UI/GameFontCatalog.cpp
        UI/ModMenu.cpp
        UI/Overlay.cpp
        UI/OverlayScriptImGui.cpp
        UI/Gui/Button.cpp
        UI/Gui/Element.cpp
        UI/Gui/Gui.cpp
        UI/Gui/Input.cpp
        UI/Gui/KeyInput.cpp
        UI/Gui/Label.cpp
        UI/Gui/Panel.cpp
        UI/Gui/Text.cpp
)

set(BML_HOOK_PRIVATE_HEADERS
        Hooks/CKRasterizer.h
        Hooks/CKRasterizerEnums.h
        Hooks/CKRasterizerTypes.h
        Hooks/Macros.h
        Hooks/RenderHook.h
        Hooks/VTables.h
)

set(BML_HOOK_SOURCES
        Hooks/InputHook.cpp
        Hooks/ObjectLoadHook.cpp
        Hooks/PhysicsHook.cpp
        Hooks/RenderHook.cpp
)

set(BML_BEHAVIOR_PRIVATE_HEADERS
        Behavior/Authoring.h
        Behavior/Callback.h
        Behavior/Execution.h
        Behavior/Lifecycle.h
        Behavior/OutcomeStore.h
        Behavior/HookBlock.h
        Behavior/ObjectLoad.h
        Behavior/Physicalize.h
        Behavior/PhysicsForce.h
        Behavior/PhysicsImpulse.h
        Behavior/PhysicsWakeUp.h
        Behavior/Runtime.h
        Behavior/SendMessage.h
        Behavior/Text2D.h
)

set(BML_BEHAVIOR_SOURCES
        Behavior/Authoring.cpp
        Behavior/Callback.cpp
        Behavior/Execution.cpp
        Behavior/Lifecycle.cpp
        Behavior/OutcomeStore.cpp
        Behavior/HookBlock.cpp
        Behavior/ObjectLoad.cpp
        Behavior/Physicalize.cpp
        Behavior/PhysicsForce.cpp
        Behavior/PhysicsImpulse.cpp
        Behavior/PhysicsWakeUp.cpp
        Behavior/Runtime.cpp
        Behavior/SendMessage.cpp
        Behavior/Text2D.cpp
)

set(BML_VIRTOOLS_SOURCES
        Virtools/ScriptHelper.cpp
)

set(BML_PRIVATE_HEADERS
        ${BML_MODS_PRIVATE_HEADERS}
        ${BML_LOADER_PRIVATE_HEADERS}
        ${BML_API_PRIVATE_HEADERS}
        ${BML_DATA_SHARE_PRIVATE_HEADERS}
        ${BML_LOGGING_PRIVATE_HEADERS}
        ${BML_CONFIG_PRIVATE_HEADERS}
        ${BML_CONSOLE_PRIVATE_HEADERS}
        ${BML_HUD_PRIVATE_HEADERS}
        ${BML_CUSTOM_MAPS_PRIVATE_HEADERS}
        ${BML_GAMEPLAY_PRIVATE_HEADERS}
        ${BML_IMC_PRIVATE_HEADERS}
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
        ${BML_LOGGING_SOURCES}
        ${BML_CONFIG_SOURCES}
        ${BML_CONSOLE_SOURCES}
        ${BML_HUD_SOURCES}
        ${BML_CUSTOM_MAPS_SOURCES}
        ${BML_GAMEPLAY_SOURCES}
        ${BML_IMC_SOURCES}
        ${BML_UI_SOURCES}
        ${BML_HOOK_SOURCES}
        ${BML_BEHAVIOR_SOURCES}
        ${BML_VIRTOOLS_SOURCES}
        BML.rc
)

set_source_files_properties(BML.rc PROPERTIES
        OBJECT_DEPENDS "${BML_VERSION_HEADER}"
)

set(BML_ANGELSCRIPT_PRIVATE_HEADERS)
set(BML_ANGELSCRIPT_SOURCES)
if (BML_WITH_ANGELSCRIPT)
    set(BML_ANGELSCRIPT_PRIVATE_HEADERS
            AngelScript/CKAngelScriptAdapter.h
            AngelScript/AngelScriptBindings.h
            AngelScript/AngelScriptImGuiBindings.h
            AngelScript/ScriptApiSurface.h
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
            AngelScript/ScriptLibraryRegistry.h
            AngelScript/ScriptLibraryServices.h
            AngelScript/ScriptLibraryTools.h
            AngelScript/ScriptLibraryValidator.h
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
    set(BML_ANGELSCRIPT_SOURCES
            AngelScript/AngelScriptBindings.cpp
            AngelScript/CKAngelScriptAdapter.cpp
            AngelScript/ScriptApiSurface.cpp
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
            AngelScript/ScriptLibraryRegistry.cpp
            AngelScript/ScriptLibraryServices.cpp
            AngelScript/ScriptLibraryTools.cpp
            AngelScript/ScriptLibraryValidator.cpp
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
)

target_compile_features(BML PUBLIC cxx_std_20)

target_include_directories(BML
        PUBLIC
        $<BUILD_INTERFACE:${BML_INCLUDE_DIR}>
        $<BUILD_INTERFACE:${IMGUI_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
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
        minhook onig
)

add_dependencies(BML BML_VersionHeader)

target_compile_definitions(BML PRIVATE
        BML_EXPORTS IMGUI_EXPORT IMGUI_USER_CONFIG=\"BMLImGuiConfig.h\"
        BML_ENABLE_ANGELSCRIPT=$<BOOL:${BML_WITH_ANGELSCRIPT}>
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
        "${PROJECT_SOURCE_DIR}/cmake/BMLImc.cmake"
        "${PROJECT_SOURCE_DIR}/cmake/FindVirtoolsSDK.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/BML"
)

install(FILES
        "${PROJECT_SOURCE_DIR}/tools/imc_codegen.py"
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
        DESTINATION "templates"
)

install(FILES
        "${PROJECT_SOURCE_DIR}/scripts/New-BMLNativeMod.ps1"
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
        "${PROJECT_SOURCE_DIR}/docs/en/imc-author-guide.md"
        "${PROJECT_SOURCE_DIR}/docs/en/imc.md"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/BML/docs/en"
)

install(FILES
        "${PROJECT_SOURCE_DIR}/docs/zh-CN/modding.md"
        "${PROJECT_SOURCE_DIR}/docs/zh-CN/native-mod-api.md"
        "${PROJECT_SOURCE_DIR}/docs/zh-CN/native-api-routes.md"
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

    install(FILES
            "${PROJECT_SOURCE_DIR}/scripts/New-BMLScriptMod.ps1"
            "${PROJECT_SOURCE_DIR}/scripts/Pack-BMLScriptMod.ps1"
            DESTINATION "scripts"
    )
endif ()
