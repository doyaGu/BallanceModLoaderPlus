if (BML_WITH_ANGELSCRIPT)
    if (WIN32)
        add_bml_test(ScriptFileWatcherWin32Test
                SOURCES
                ScriptFileWatcherWin32Test.cpp
                ${BML_SOURCE_DIR}/AngelScript/ScriptFileWatcherWin32.cpp
        )

        add_bml_test(ScriptModHotReloadPathFilterTest
                SOURCES
                ScriptModHotReloadPathFilterTest.cpp
                ${BML_SOURCE_DIR}/AngelScript/ScriptModHotReloadPathFilter.cpp
                DEPENDENCIES
                BML_PathUtils
        )
    endif ()

    add_bml_test(ScriptServiceLifecycleTest
            SOURCES
            ScriptServiceLifecycleTest.cpp
            ScriptServiceLifecycleStubs.cpp
            ${BML_SOURCE_DIR}/AngelScript/ScriptCommandService.cpp
            ${BML_SOURCE_DIR}/AngelScript/ScriptDataShareService.cpp
            ${BML_SOURCE_DIR}/AngelScript/ScriptDiagnostic.cpp
            ${BML_SOURCE_DIR}/AngelScript/ScriptFunctionSupport.cpp
            ${BML_SOURCE_DIR}/AngelScript/ScriptStringInterop.cpp
            ${BML_SOURCE_DIR}/AngelScript/ScriptTimerService.cpp
            ${BML_SOURCE_DIR}/Console/Shell/ShellIo.cpp
            ${BML_SOURCE_DIR}/DataShare/DataShare.cpp
            DEPENDENCIES
            BML_StringUtils
    )
    target_compile_options(ScriptServiceLifecycleTest PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/Gy>
    )
    target_link_options(ScriptServiceLifecycleTest PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/OPT:REF>
    )

    add_bml_test(ScriptModStateTest
            SOURCES
            ScriptModStateTest.cpp
            ${BML_SOURCE_DIR}/AngelScript/CKAngelScriptAdapter.cpp
            ${BML_SOURCE_DIR}/AngelScript/ScriptDiagnostic.cpp
            ${BML_SOURCE_DIR}/AngelScript/ScriptModState.cpp
    )

    add_bml_test(ScriptModDefinitionTest
            SOURCES
            ScriptModDefinitionTest.cpp
            ${BML_SOURCE_DIR}/AngelScript/ScriptModDefinition.cpp
            DEPENDENCIES
            BML_StringUtils
    )

    add_bml_test(ScriptModRuntimeTest
            SOURCES
            ScriptModRuntimeTest.cpp
            ScriptModRuntimeTestStubs.cpp
            ${BML_SOURCE_DIR}/AngelScript/CKAngelScriptAdapter.cpp
            ${BML_SOURCE_DIR}/AngelScript/ScriptDiagnostic.cpp
            ${BML_SOURCE_DIR}/AngelScript/ScriptModLifecycle.cpp
            ${BML_SOURCE_DIR}/AngelScript/ScriptModRuntime.cpp
    )
    target_compile_definitions(ScriptModRuntimeTest PRIVATE BML_SCRIPT_RUNTIME_TEST_ACCESS)

    add_bml_test(ScriptLibraryRegistryTest
            SOURCES
            ScriptLibraryRegistryTest.cpp
            ${BML_SOURCE_DIR}/AngelScript/CKAngelScriptAdapter.cpp
            ${BML_SOURCE_DIR}/AngelScript/ScriptDiagnostic.cpp
            ${BML_SOURCE_DIR}/AngelScript/ScriptLibraryRegistry.cpp
            ${BML_SOURCE_DIR}/AngelScript/ScriptLibraryTools.cpp
            ${BML_SOURCE_DIR}/AngelScript/ScriptLibraryValidator.cpp
            ${BML_SOURCE_DIR}/AngelScript/ScriptModEntryScanner.cpp
            ${BML_SOURCE_DIR}/AngelScript/ScriptSourceSnapshotBuilder.cpp
            DEPENDENCIES
            BML_PathUtils
            BML_StringUtils
            BML_CryptoUtils
    )

    add_bml_test(ScriptSourceSnapshotBuilderTest
            SOURCES
            ScriptSourceSnapshotBuilderTest.cpp
            ${BML_SOURCE_DIR}/AngelScript/ScriptLibraryRegistry.cpp
            ${BML_SOURCE_DIR}/AngelScript/ScriptSourceSnapshotBuilder.cpp
            ${BML_SOURCE_DIR}/AngelScript/CKAngelScriptAdapter.cpp
            ${BML_SOURCE_DIR}/AngelScript/ScriptDiagnostic.cpp
            ${BML_SOURCE_DIR}/AngelScript/ScriptModEntryScanner.cpp
            DEPENDENCIES
            BML_PathUtils
            BML_StringUtils
            BML_CryptoUtils
    )

    add_bml_test(ScriptStateBagTest
            SOURCES
            ScriptStateBagTest.cpp
            ${BML_SOURCE_DIR}/AngelScript/ScriptStateBag.cpp
    )

    add_bml_test(ScriptReloadStagingCleanupTest
            SOURCES
            ScriptReloadStagingCleanupTest.cpp
            ${BML_SOURCE_DIR}/AngelScript/ScriptReloadStagingCleanup.cpp
            DEPENDENCIES
            BML_PathUtils
            BML_StringUtils
    )
endif ()

add_bml_test(ScriptDevEventsTest
        SOURCES
        ScriptDevEventsTest.cpp
        ${BML_SOURCE_DIR}/AngelScript/ScriptDevEvents.cpp
        DEPENDENCIES
        BML_StringUtils
)
