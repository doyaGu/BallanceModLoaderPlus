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

# .imc files are authoring inputs only. This target verifies the committed
# typed IMC headers and prevents a runtime dependency on an IDL parser.
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
set(BML_IMC_API_INPUTS
        ${PROJECT_SOURCE_DIR}/imc/interfaces/bml.gameplay.imc
        ${PROJECT_SOURCE_DIR}/imc/interfaces/bml.events.imc
)
set(BML_IMC_LOCK_INPUTS
        ${PROJECT_SOURCE_DIR}/imc/interfaces/bml.gameplay.imc.lock
        ${PROJECT_SOURCE_DIR}/imc/interfaces/bml.events.imc.lock
)
add_custom_target(BML_ImcApiGeneration
        COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/imc_codegen.py
                --check
                --out-dir ${BML_INCLUDE_DIR}/BML/Generated
                --input ${PROJECT_SOURCE_DIR}/imc/interfaces/bml.gameplay.imc
                --input ${PROJECT_SOURCE_DIR}/imc/interfaces/bml.events.imc
        DEPENDS
                ${PROJECT_SOURCE_DIR}/tools/imc_codegen.py
                ${BML_IMC_API_INPUTS}
                ${BML_IMC_LOCK_INPUTS}
        COMMENT "Verifying generated IMC API bindings"
)

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
        ${BML_INCLUDE_DIR}/BML/ImcMath.h
        ${BML_INCLUDE_DIR}/BML/EventKinds.h
        ${BML_INCLUDE_DIR}/BML/Generated/bml_gameplay_imc.hpp
        ${BML_INCLUDE_DIR}/BML/Generated/bml_events_imc.hpp
        ${BML_INCLUDE_DIR}/BML/Runtime.h
        ${BML_INCLUDE_DIR}/BML/Scene.h
        ${BML_INCLUDE_DIR}/BML/Gameplay.h
        ${BML_INCLUDE_DIR}/BML/UI.h
        ${BML_INCLUDE_DIR}/BML/Speedrun.h
        ${BML_INCLUDE_DIR}/BML/Events.h

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

set(BML_PRIVATE_HEADERS
        ModManager.h
        ModContext.h
        BuiltinImcApis.h
        InterfaceRegistry.h
        RuntimeState.h

        BMLMod.h
        NewBallTypeMod.h
        Commands.h

        HUD.h
        FpsCounter.h
        SRTimer.h
        RealTimer.h
        ModMenu.h
        MapMenu.h
        CommandBar.h
        MessageBoard.h
        AnsiPalette.h
        AnsiText.h
        EventHook.h
        ImGuiStateRecovery.h

        Overlay.h

        RenderHook.h
        ImcPrimitives.h
        ImcRuntime.h
        ImcEventSnapshot.h
        ImcObjectReferenceRegistry.h

        CommandContext.h
        DataShare.hpp
        Logger.h
        Config.h
)

set(BML_SOURCES
        BML.cpp
        Errors.cpp

        HookBlock.cpp
        ModManager.cpp
        ModContext.cpp
        BuiltinImcApis.cpp
        InterfaceRegistry.cpp
        Interfaces.cpp
        RuntimeState.cpp

        IMod.cpp
        BMLMod.cpp
        NewBallTypeMod.cpp
        Commands.cpp

        HUD.cpp
        FpsCounter.cpp
        SRTimer.cpp
        RealTimer.cpp
        ModMenu.cpp
        MapMenu.cpp
        CommandBar.cpp
        MessageBoard.cpp
        AnsiPalette.cpp
        AnsiText.cpp
        EventHook.cpp
        ImGuiStateRecovery.cpp

        Gui/Element.cpp
        Gui/Text.cpp
        Gui/Panel.cpp
        Gui/Label.cpp
        Gui/Button.cpp
        Gui/Input.cpp
        Gui/KeyInput.cpp
        Gui/Gui.cpp

        Overlay.cpp
        Bui.cpp

        RenderHook.cpp
        InputHook.cpp
        ObjectLoadHook.cpp
        PhysicsHook.cpp

        ExecuteBB.cpp
        ScriptHelper.cpp

        CommandContext.cpp
        ImcApi.cpp
        ImcRuntime.cpp
        DataShare.cpp
        Logger.cpp
        Config.cpp

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
            AngelScript/ScriptImcFacade.h
            AngelScript/ScriptImcClients.h
            AngelScript/ScriptModDefinition.h
            AngelScript/ScriptModDefinitionBuilder.h
            AngelScript/ScriptModEntryScanner.h
            AngelScript/ScriptModEventRouter.h
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
            AngelScript/ScriptImcFacade.cpp
            AngelScript/ScriptImcClients.cpp
            AngelScript/ScriptModDefinition.cpp
            AngelScript/ScriptModDefinitionBuilder.cpp
            AngelScript/ScriptModEntryScanner.cpp
            AngelScript/ScriptModEventRouter.cpp
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
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui_impl_ck2.h
)

set(IMGUI_SOURCES
        ${IMGUI_SOURCE_DIR}/imgui.cpp
        ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
        ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
        ${IMGUI_SOURCE_DIR}/imgui_demo.cpp
        ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
        ${IMGUI_SOURCE_DIR}/backends/imgui_impl_win32.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui_impl_ck2.cpp
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
add_dependencies(BML BML_ImcApiGeneration)

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
