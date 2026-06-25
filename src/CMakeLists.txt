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
    COMMENT "Generating Version.h with git metadata"
)

add_subdirectory(Utils)

set(BML_PUBLIC_HEADERS
        ${BML_VERSION_HEADER}
        ${BML_INCLUDE_DIR}/BML/Defines.h
        ${BML_INCLUDE_DIR}/BML/BML.h

        ${BML_INCLUDE_DIR}/BML/ILogger.h
        ${BML_INCLUDE_DIR}/BML/ICommand.h
        ${BML_INCLUDE_DIR}/BML/IConfig.h
        ${BML_INCLUDE_DIR}/BML/IMessageReceiver.h
        ${BML_INCLUDE_DIR}/BML/IBML.h
        ${BML_INCLUDE_DIR}/BML/IMod.h
        ${BML_INCLUDE_DIR}/BML/DataShare.h
        ${BML_INCLUDE_DIR}/BML/Interop.h
        ${BML_INCLUDE_DIR}/BML/Import.h
        ${BML_INCLUDE_DIR}/BML/Core.h

        ${BML_INCLUDE_DIR}/BML/Bui.h
        ${BML_INCLUDE_DIR}/BML/Gui.h
        ${BML_INCLUDE_DIR}/BML/InputHook.h

        ${BML_INCLUDE_DIR}/BML/ExecuteBB.h
        ${BML_INCLUDE_DIR}/BML/ScriptHelper.h

        ${BML_INCLUDE_DIR}/BML/BMLAll.h
)

set(BML_PRIVATE_HEADERS
        ModManager.h
        ModContext.h
        BuiltinCapabilityImports.h

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

        Overlay.h

        RenderHook.h
        CallFrameInternal.h
        InteropSignature.h
        ExportRegistry.h

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
        BuiltinCapabilityImports.cpp

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
        CallFrameInternal.cpp
        InteropSignature.cpp
        DataShare.cpp
        ExportRegistry.cpp
        Interop.cpp
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
            AngelScript/ScriptApiContract.h
            AngelScript/ScriptAngelScriptHandle.h
            AngelScript/ScriptCallbackDispatcher.h
            AngelScript/ScriptCallbackEvents.h
            AngelScript/ScriptCommandService.h
            AngelScript/ScriptDataShareService.h
            AngelScript/ScriptDevEvents.h
            AngelScript/ScriptDevToolsService.h
            AngelScript/ScriptDiagnostic.h
            AngelScript/ScriptExportDispatcher.h
            AngelScript/ScriptExportSignature.h
            AngelScript/ScriptFacadeAccess.h
            AngelScript/ScriptFileWatcherWin32.h
            AngelScript/ScriptHookBlockService.h
            AngelScript/ScriptLibraryRegistry.h
            AngelScript/ScriptLibraryServices.h
            AngelScript/ScriptLibraryValidator.h
            AngelScript/ScriptMod.h
            AngelScript/ScriptModContextView.h
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
            AngelScript/ScriptApiContract.cpp
            AngelScript/ScriptCallbackDispatcher.cpp
            AngelScript/ScriptCallbackEvents.cpp
            AngelScript/ScriptCommandService.cpp
            AngelScript/ScriptDataShareService.cpp
            AngelScript/ScriptDevEvents.cpp
            AngelScript/ScriptDevToolsService.cpp
            AngelScript/ScriptDiagnostic.cpp
            AngelScript/ScriptExportDispatcher.cpp
            AngelScript/ScriptExportSignature.cpp
            AngelScript/ScriptFileWatcherWin32.cpp
            AngelScript/ScriptFunctionSupport.cpp
            AngelScript/ScriptHookBlockService.cpp
            AngelScript/ScriptLibraryRegistry.cpp
            AngelScript/ScriptLibraryServices.cpp
            AngelScript/ScriptLibraryValidator.cpp
            AngelScript/ScriptMod.cpp
            AngelScript/ScriptModContextView.cpp
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
        CK2 VxMath
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
        FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(DIRECTORY "${BML_INCLUDE_DIR}/BML"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

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
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/BML"
)
