include(CTest)
enable_testing()
find_package(Python3 REQUIRED COMPONENTS Interpreter)

include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.16.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

function(add_bml_test test_name)
    cmake_parse_arguments(TEST "" "" "SOURCES;DEPENDENCIES" ${ARGN})

    add_executable(${test_name} ${TEST_SOURCES})

    target_include_directories(${test_name} PRIVATE
            "${BML_INCLUDE_DIR}"
            "${BML_SOURCE_DIR}"
            "${BML_SOURCE_DIR}/Utils"
    )
    if (BML_WITH_ANGELSCRIPT)
        target_include_directories(${test_name} PRIVATE
                "${BML_SOURCE_DIR}/AngelScript"
                "${VIRTOOLS_SDK_INCLUDE_DIR}"
                "${CKANGELSCRIPT_INCLUDE_DIR}"
                "${CKANGELSCRIPT_ANGELSCRIPT_INCLUDE_DIR}"
        )
    endif ()

    target_link_libraries(${test_name} PRIVATE
            gtest_main
            gmock
            ${TEST_DEPENDENCIES}
    )

    target_compile_definitions(${test_name} PRIVATE
            "BML_EXPORTS"
            "BML_TEST"
            "IMGUI_USER_CONFIG=\"BMLImGuiConfig.h\""
            "BML_ENABLE_ANGELSCRIPT=$<BOOL:${BML_WITH_ANGELSCRIPT}>"
    )
    target_compile_options(${test_name} PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/Zc:strictStrings->
    )

    set_target_properties(${test_name} PROPERTIES FOLDER "Tests")

    add_test(NAME ${test_name} COMMAND $<TARGET_FILE:${test_name}>)
endfunction()

# Build the public Interop C headers as strict C.  This is deliberately an
# object target: it verifies source compatibility without introducing a test
# executable or a C++ link boundary.
add_library(InteropCAbiCompile OBJECT InteropCAbiCompile.c)
target_include_directories(InteropCAbiCompile PRIVATE "${BML_INCLUDE_DIR}")
set_target_properties(InteropCAbiCompile PROPERTIES FOLDER "Tests")

# Keep the IDL generator's compatibility rules executable.  This also checks
# that generated native helpers remain header-only and the script artifact is
# usable code rather than documentation prose.
add_test(
        NAME InteropCodegenTest
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/InteropCodegenTest.py"
                --source-root "${PROJECT_SOURCE_DIR}"
)
set_tests_properties(InteropCodegenTest PROPERTIES LABELS "interop;codegen")

add_test(
        NAME InteropPublicSurfaceTest
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/InteropPublicSurfaceTest.py"
                --source-root "${PROJECT_SOURCE_DIR}"
)
set_tests_properties(InteropPublicSurfaceTest PROPERTIES LABELS "interop;abi")

# The headers above prove C source compatibility.  On MSVC also inspect the
# linked DLL, where exported C++ helper symbols would otherwise go unnoticed.
if(MSVC)
    get_filename_component(BML_TOOL_DIRECTORY "${CMAKE_LINKER}" DIRECTORY)
    set(BML_DUMPBIN "${BML_TOOL_DIRECTORY}/dumpbin.exe")
    if(EXISTS "${BML_DUMPBIN}")
        add_test(
                NAME InteropDllExportAbiTest
                COMMAND "${CMAKE_COMMAND}"
                "-DDUMPBIN=${BML_DUMPBIN}"
                "-DBINARY=$<TARGET_FILE:BML>"
                "-P" "${CMAKE_CURRENT_SOURCE_DIR}/InteropDllExportAbiTest.cmake"
        )
        set_tests_properties(InteropDllExportAbiTest PROPERTIES
                LABELS "interop;abi"
        )
    endif()
endif()

add_bml_test(TimerTest
        SOURCES TimerTest.cpp
)

add_bml_test(StringUtilsTest
        SOURCES
        StringUtilsTest.cpp
        DEPENDENCIES
        BMLUtils
)

add_bml_test(PathUtilsTest
        SOURCES
        PathUtilsTest.cpp
        DEPENDENCIES
        BMLUtils
)

add_bml_test(ConfigTest
        SOURCES
        ConfigTest.cpp
        ${BML_SOURCE_DIR}/Config.cpp
        ${BML_SOURCE_DIR}/Config.h
        DEPENDENCIES
        BMLUtils
        CK2
        VxMath
)

add_bml_test(InteropObjectReferenceRegistryTest
        SOURCES
        InteropObjectReferenceRegistryTest.cpp
)

add_bml_test(IniFileTest
        SOURCES
        IniFileTest.cpp
        DEPENDENCIES
        BMLUtils
)

if (WIN32)
    add_bml_test(HookUtilsTest
            SOURCES
            HookUtilsTest.cpp
            DEPENDENCIES
            BML_HookUtils
    )

    add_bml_test(CryptoUtilsTest
            SOURCES
            CryptoUtilsTest.cpp
            DEPENDENCIES
            BML_CryptoUtils
            BML_PathUtils
            BML_StringUtils
    )
endif ()

add_bml_test(AnsiPaletteTest
        SOURCES
        AnsiPaletteTest.cpp
        ${BML_SOURCE_DIR}/AnsiPalette.cpp
        DEPENDENCIES
        BMLUtils
)
# Need ImGui headers for AnsiPalette
target_include_directories(AnsiPaletteTest PRIVATE ${IMGUI_SOURCE_DIR})

add_bml_test(AnsiTextTest
        SOURCES
        AnsiTextTest.cpp
        ${BML_SOURCE_DIR}/AnsiText.cpp
        ${BML_SOURCE_DIR}/AnsiPalette.cpp
        ${IMGUI_SOURCE_DIR}/imgui.cpp
        ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
        ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
        ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
        DEPENDENCIES
        BMLUtils
)
target_include_directories(AnsiTextTest PRIVATE ${IMGUI_SOURCE_DIR})
target_compile_definitions(AnsiTextTest PRIVATE IMGUI_EXPORT)

add_bml_test(ImGuiStateRecoveryTest
        SOURCES
        ImGuiStateRecoveryTest.cpp
        ${BML_SOURCE_DIR}/ImGuiStateRecovery.cpp
        ${IMGUI_SOURCE_DIR}/imgui.cpp
        ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
        ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
        ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
)
target_include_directories(ImGuiStateRecoveryTest PRIVATE ${IMGUI_SOURCE_DIR})
target_compile_definitions(ImGuiStateRecoveryTest PRIVATE IMGUI_EXPORT)

add_bml_test(FpsCounterTest
        SOURCES
        FpsCounterTest.cpp
        ${BML_SOURCE_DIR}/FpsCounter.cpp
)

add_bml_test(SRTimerTest
        SOURCES
        SRTimerTest.cpp
        ${BML_SOURCE_DIR}/SRTimer.cpp
)

add_bml_test(CommandContextTest
        SOURCES
        CommandContextTest.cpp
        ${BML_SOURCE_DIR}/CommandContext.cpp
        DEPENDENCIES
        BMLUtils
)

add_bml_test(DataShareTest
        SOURCES
        DataShareTest.cpp
        ${BML_SOURCE_DIR}/DataShare.cpp
)

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
            ${BML_SOURCE_DIR}/DataShare.cpp
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

    add_bml_test(ScriptApiReferenceTest
            SOURCES
            ScriptApiReferenceTest.cpp
    )
    target_compile_definitions(ScriptApiReferenceTest PRIVATE
            "BML_TEST_SOURCE_ROOT=\"${PROJECT_SOURCE_DIR}\""
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

add_bml_test(ModInvocationGateTest
        SOURCES
        ModInvocationGateTest.cpp
)

add_bml_test(InteropErrorTest
        SOURCES
        InteropErrorTest.cpp
        ${BML_SOURCE_DIR}/Errors.cpp
)

add_bml_test(InteropSessionServiceTest
        SOURCES
        InteropSessionServiceTest.cpp
        ${BML_SOURCE_DIR}/InteropSessionService.cpp
)

add_bml_test(InteropRegistryTest
        SOURCES
        InteropRegistryTest.cpp
        ${BML_SOURCE_DIR}/InteropRegistry.cpp
        ${BML_SOURCE_DIR}/InteropSessionService.cpp
        DEPENDENCIES
        BMLUtils
)

add_bml_test(InteropHeadersTest
        SOURCES
        InteropHeadersTest.cpp
)

add_bml_test(InteropMathTest
        SOURCES
        InteropMathTest.cpp
        DEPENDENCIES
        VxMath
)

add_subdirectory(smoke)
if (BML_BUILD_UPDATER)
    add_subdirectory(updater)
endif ()
