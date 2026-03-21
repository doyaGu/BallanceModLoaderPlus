include(CTest)
enable_testing()

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
    )

    target_link_libraries(${test_name} PRIVATE
            gtest_main
            gmock
            ${TEST_DEPENDENCIES}
    )

    target_compile_definitions(${test_name} PRIVATE "BML_EXPORTS" "BML_TEST")

    set_target_properties(${test_name} PROPERTIES FOLDER "Tests")

    add_test(NAME ${test_name} COMMAND $<TARGET_FILE:${test_name}>)
endfunction()

function(add_bml_test_support_library target_name)
    cmake_parse_arguments(SUPPORT "" "" "SOURCES;DEPENDENCIES" ${ARGN})

    add_library(${target_name} STATIC ${SUPPORT_SOURCES})

    target_include_directories(${target_name} PUBLIC
            "${BML_INCLUDE_DIR}"
            "${BML_SOURCE_DIR}"
    )

    target_link_libraries(${target_name} PUBLIC ${SUPPORT_DEPENDENCIES})
    target_compile_definitions(${target_name} PUBLIC "BML_BUILD_DLL" "BML_EXPORTS" "BML_TEST")

    set_target_properties(${target_name} PROPERTIES
            FOLDER "Tests/Support"
    )
endfunction()

if (WIN32)
    set(BML_SYNC_MANAGER_SOURCE ${BML_SOURCE_DIR}/Core/SyncManager.cpp)
else ()
    set(BML_SYNC_MANAGER_SOURCE ${BML_SOURCE_DIR}/Core/SyncManagerPosix.cpp)
endif ()

add_bml_test_support_library(bml_test_runtime_support
        SOURCES
        ModuleLoaderStub.cpp
        TestKernelSupport.cpp
        ExportStub.cpp
        ${BML_SOURCE_DIR}/Core/ApiRegistry.cpp
        ${BML_SOURCE_DIR}/Core/BuiltinInterfaces.cpp
        ${BML_SOURCE_DIR}/Core/ConfigApi.cpp
        ${BML_SOURCE_DIR}/Core/ConfigStore.cpp
        ${BML_SOURCE_DIR}/Core/Context.cpp
        ${BML_SOURCE_DIR}/Core/CoreApi.cpp
        ${BML_SOURCE_DIR}/Core/CoreErrors.cpp
        ${BML_SOURCE_DIR}/Core/CrashDumpWriter.cpp
        ${BML_SOURCE_DIR}/Core/DependencyResolver.cpp
        ${BML_SOURCE_DIR}/Core/DiagnosticApi.cpp
        ${BML_SOURCE_DIR}/Core/DiagnosticManager.cpp
        ${BML_SOURCE_DIR}/Core/FileSystemWatcher.cpp
        ${BML_SOURCE_DIR}/Core/FaultTracker.cpp
        ${BML_SOURCE_DIR}/Core/HookApi.cpp
        ${BML_SOURCE_DIR}/Core/HookRegistry.cpp
        ${BML_SOURCE_DIR}/Core/HotReloadCoordinator.cpp
        ${BML_SOURCE_DIR}/Core/ImcBus.cpp
        ${BML_SOURCE_DIR}/Core/InterfaceApi.cpp
        ${BML_SOURCE_DIR}/Core/InterfaceRegistry.cpp
        ${BML_SOURCE_DIR}/Core/LeaseManager.cpp
        ${BML_SOURCE_DIR}/Core/LocaleApi.cpp
        ${BML_SOURCE_DIR}/Core/LocaleManager.cpp
        ${BML_SOURCE_DIR}/Core/Logging.cpp
        ${BML_SOURCE_DIR}/Core/LoggingApi.cpp
        ${BML_SOURCE_DIR}/Core/MemoryApi.cpp
        ${BML_SOURCE_DIR}/Core/MemoryManager.cpp
        ${BML_SOURCE_DIR}/Core/ModManifest.cpp
        ${BML_SOURCE_DIR}/Core/ModuleDiscovery.cpp
        ${BML_SOURCE_DIR}/Core/ModuleLifecycle.cpp
        ${BML_SOURCE_DIR}/Core/ProfilingApi.cpp
        ${BML_SOURCE_DIR}/Core/ProfilingManager.cpp
        ${BML_SOURCE_DIR}/Core/ReloadableModuleSlot.cpp
        ${BML_SOURCE_DIR}/Core/ResourceApi.cpp
        ${BML_SOURCE_DIR}/Core/SemanticVersion.cpp
        ${BML_SYNC_MANAGER_SOURCE}
        ${BML_SOURCE_DIR}/Core/SyncApi.cpp
        ${BML_SOURCE_DIR}/Core/TimerApi.cpp
        ${BML_SOURCE_DIR}/Core/TimerManager.cpp
        ${BML_SOURCE_DIR}/Core/TracingApi.cpp
        DEPENDENCIES
        BML_Utils
        tomlplusplus::tomlplusplus
        efsw
)

add_bml_test_support_library(bml_test_core_support
        SOURCES
        ModuleLoaderStub.cpp
        TestKernelSupport.cpp
        ${BML_SOURCE_DIR}/Core/ApiRegistry.cpp
        ${BML_SOURCE_DIR}/Core/ConfigStore.cpp
        ${BML_SOURCE_DIR}/Core/Context.cpp
        ${BML_SOURCE_DIR}/Core/CoreErrors.cpp
        ${BML_SOURCE_DIR}/Core/DiagnosticManager.cpp
        ${BML_SOURCE_DIR}/Core/Logging.cpp
        ${BML_SOURCE_DIR}/Core/ResourceApi.cpp
        DEPENDENCIES
        BML_Utils
        tomlplusplus::tomlplusplus
)

add_bml_test_support_library(bml_test_config_api_support
        SOURCES
        ${BML_SOURCE_DIR}/Core/ConfigApi.cpp
        DEPENDENCIES
        bml_test_core_support
)

add_bml_test_support_library(bml_test_logging_api_support
        SOURCES
        ${BML_SOURCE_DIR}/Core/LoggingApi.cpp
        DEPENDENCIES
        bml_test_core_support
)

add_bml_test_support_library(bml_test_imc_support
        SOURCES
        ${BML_SOURCE_DIR}/Core/CrashDumpWriter.cpp
        ${BML_SOURCE_DIR}/Core/FaultTracker.cpp
        ${BML_SOURCE_DIR}/Core/ImcBus.cpp
        DEPENDENCIES
        bml_test_core_support
)

add_bml_test_support_library(bml_test_manifest_support
        SOURCES
        ${BML_SOURCE_DIR}/Core/ModManifest.cpp
        ${BML_SOURCE_DIR}/Core/SemanticVersion.cpp
        DEPENDENCIES
        BML_Utils
        tomlplusplus::tomlplusplus
)

add_bml_test_support_library(bml_test_stub_logging_support
        SOURCES
        TestLoggingStub.cpp
        ${BML_SOURCE_DIR}/Core/CoreErrors.cpp
        ${BML_SOURCE_DIR}/Core/DiagnosticManager.cpp
        ${BML_SOURCE_DIR}/Core/KernelServices.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/TestKernelSupport.cpp
        DEPENDENCIES
        BML_Utils
        tomlplusplus::tomlplusplus
)

add_bml_test_support_library(bml_test_dependency_support
        SOURCES
        ${BML_SOURCE_DIR}/Core/DependencyResolver.cpp
        DEPENDENCIES
        bml_test_manifest_support
        bml_test_stub_logging_support
)

add_bml_test_support_library(bml_test_module_discovery_support
        SOURCES
        ${BML_SOURCE_DIR}/Core/ModuleDiscovery.cpp
        DEPENDENCIES
        bml_test_manifest_support
        bml_test_dependency_support
)

add_bml_test_support_library(bml_test_filewatcher_support
        SOURCES
        ${BML_SOURCE_DIR}/Core/FileSystemWatcher.cpp
        DEPENDENCIES
        bml_test_stub_logging_support
        efsw
)

add_bml_test_support_library(bml_test_module_loader_support
        SOURCES
        ${BML_SOURCE_DIR}/Core/DependencyResolver.cpp
        DEPENDENCIES
        bml_test_core_support
        bml_test_manifest_support
)

add_bml_test_support_library(bml_test_profiling_support
        SOURCES
        ${BML_SOURCE_DIR}/Core/MemoryManager.cpp
        ${BML_SOURCE_DIR}/Core/ProfilingManager.cpp
        ${BML_SOURCE_DIR}/Core/TracingApi.cpp
        DEPENDENCIES
        bml_test_core_support
)

if (NOT WIN32)

    add_bml_test(CoreApisTests
            SOURCES
            CoreApisTests.cpp
            DEPENDENCIES
            bml_test_runtime_support
    )

    add_bml_test(SyncPrimitivesTest
            SOURCES
            SyncPrimitivesTest.cpp
            DEPENDENCIES
            bml_test_runtime_support
    )

    add_bml_test(SyncManagerTests
            SOURCES
            SyncManagerTests.cpp
            DEPENDENCIES
            bml_test_runtime_support
    )

    add_bml_test(ProfilingManagerTests
            SOURCES
            ProfilingManagerTests.cpp
            DEPENDENCIES
            bml_test_runtime_support
    )

    add_bml_test(ReloadableModuleSlotTests
            SOURCES
            ReloadableModuleSlotTests.cpp
            DEPENDENCIES
            bml_test_runtime_support
    )

    add_bml_test(HotReloadCoordinatorTests
            SOURCES
            HotReloadCoordinatorTests.cpp
            DEPENDENCIES
            bml_test_runtime_support
    )

    add_bml_test(LocaleManagerTests
            SOURCES
            LocaleManagerTests.cpp
            DEPENDENCIES
            bml_test_runtime_support
    )

    return()
endif ()

add_bml_test(StringUtilsTest
        SOURCES
        StringUtilsTest.cpp
        DEPENDENCIES
        BML_Utils
)

add_bml_test(PathUtilsTest
        SOURCES
        PathUtilsTest.cpp
        DEPENDENCIES
        BML_Utils
)

add_bml_test(IniFileTest
        SOURCES
        IniFileTest.cpp
        DEPENDENCIES
        BML_Utils
)

add_bml_test(ManifestParserTest
        SOURCES
        ManifestParserTest.cpp
        DEPENDENCIES
        bml_test_manifest_support
)

add_bml_test(ModuleDiscoveryTest
        SOURCES
        ModuleDiscoveryTest.cpp
        DEPENDENCIES
        bml_test_module_discovery_support
)

add_bml_test(DependencyResolverTest
        SOURCES
        DependencyResolverTest.cpp
        DEPENDENCIES
        bml_test_dependency_support
)

add_bml_test(ImcBusTest
        SOURCES
        ImcBusTest.cpp
        DEPENDENCIES
        bml_test_runtime_support
)

add_bml_test(RpcExpansionTests
        SOURCES
        RpcExpansionTests.cpp
        DEPENDENCIES
        bml_test_runtime_support
)

add_bml_test(ImcCppWrapperTest
        SOURCES
        ImcCppWrapperTest.cpp
)

add_bml_test(LoaderTest
        SOURCES
        LoaderTest.cpp
)

add_bml_test(CppWrapperTest
        SOURCES
        CppWrapperTest.cpp
)

# Struct size and enum validation tests
add_bml_test(StructSizeTest
        SOURCES
        StructSizeTest.cpp
)

add_bml_test(CoreApisTests
        SOURCES
        CoreApisTests.cpp
        DEPENDENCIES
        bml_test_runtime_support
)

add_bml_test(ApiRegistryTests
        SOURCES
        ApiRegistryTests.cpp
        DEPENDENCIES
        bml_test_core_support
)

add_bml_test(ContextLifecycleTests
        SOURCES
        ContextLifecycleTests.cpp
        DEPENDENCIES
        bml_test_core_support
)

add_bml_test(ConfigStoreConcurrencyTests
        SOURCES
        ConfigStoreConcurrencyTests.cpp
        DEPENDENCIES
        bml_test_config_api_support
)

add_bml_test(ConfigBatchApiTests
        SOURCES
        ConfigBatchApiTests.cpp
        DEPENDENCIES
        bml_test_config_api_support
)

add_bml_test(ConfigBlobTests
        SOURCES
        ConfigBlobTests.cpp
        DEPENDENCIES
        bml_test_config_api_support
)

add_bml_test(ConfigMigrationTests
        SOURCES
        ConfigMigrationTests.cpp
        DEPENDENCIES
        bml_test_config_api_support
)

add_bml_test(ResourceHandleLifecycleTests
        SOURCES
        ResourceHandleLifecycleTests.cpp
        DEPENDENCIES
        bml_test_core_support
)

add_bml_test(MemoryManagerTests
        SOURCES
        MemoryManagerTests.cpp
        DEPENDENCIES
        bml_test_profiling_support
)

add_bml_test(LoggingSinkOverrideTests
        SOURCES
        LoggingSinkOverrideTests.cpp
        DEPENDENCIES
        bml_test_logging_api_support
)

add_bml_test(CoreErrorsGuardTests
        SOURCES
        CoreErrorsGuardTests.cpp
        DEPENDENCIES
        bml_test_logging_api_support
)

# FileSystemWatcher Tests (unit tests for efsw wrapper)
add_bml_test(FileSystemWatcherTests
        SOURCES
        FileSystemWatcherTests.cpp
        DEPENDENCIES
        bml_test_filewatcher_support
)

# ReloadableModuleSlot Tests (unit tests for DLL version management)
add_bml_test(ReloadableModuleSlotTests
        SOURCES
        ReloadableModuleSlotTests.cpp
        DEPENDENCIES
        bml_test_runtime_support
)

# HotReloadCoordinator Tests (unit tests for reload coordination)
add_bml_test(HotReloadCoordinatorTests
        SOURCES
        HotReloadCoordinatorTests.cpp
        DEPENDENCIES
        bml_test_runtime_support
)

# HotReloadIntegrationTests requires full BML runtime with exported functions.
# TODO: Link to BML.dll for proper integration testing or mock bmlGetProcAddress.
# Disabled for now as it's an integration test.
#[[
add_library(HotReloadSampleMod SHARED
        HotReloadSampleMod.cpp
)

target_include_directories(HotReloadSampleMod PRIVATE
        "${BML_INCLUDE_DIR}"
)

add_bml_test(HotReloadIntegrationTests
        SOURCES
        HotReloadIntegrationTests.cpp
        ModuleLoaderStub.cpp
        ${BML_SOURCE_DIR}/Core/ApiRegistry.cpp
        ${BML_SOURCE_DIR}/Core/ConfigStore.cpp
        ${BML_SOURCE_DIR}/Core/Context.cpp
        ${BML_SOURCE_DIR}/Core/CoreErrors.cpp
        ${BML_SOURCE_DIR}/Core/DependencyResolver.cpp
        ${BML_SOURCE_DIR}/Core/FileSystemWatcher.cpp
        ${BML_SOURCE_DIR}/Core/ReloadableModuleSlot.cpp
        ${BML_SOURCE_DIR}/Core/HotReloadCoordinator.cpp
        ${BML_SOURCE_DIR}/Core/ImcBus.cpp
        ${BML_SOURCE_DIR}/Core/CrashDumpWriter.cpp
        ${BML_SOURCE_DIR}/Core/FaultTracker.cpp
        ${BML_SOURCE_DIR}/Core/Logging.cpp
        ${BML_SOURCE_DIR}/Core/ModuleDiscovery.cpp
        ${BML_SOURCE_DIR}/Core/ModuleRuntime.cpp
        ${BML_SOURCE_DIR}/Core/ModManifest.cpp
        ${BML_SOURCE_DIR}/Core/SemanticVersion.cpp
        DEPENDENCIES
        BML_Utils
        tomlplusplus::tomlplusplus
        efsw
)

add_dependencies(HotReloadIntegrationTests HotReloadSampleMod)
target_compile_definitions(HotReloadIntegrationTests PRIVATE
        BML_TEST_SAMPLE_MOD_DLL="$<TARGET_FILE:HotReloadSampleMod>")
]]

# API Improvements Test (v0.5.0 capability system, discovery, performance)
add_bml_test(ApiImprovementsTest
        SOURCES
        ApiImprovementsTest.cpp
        DEPENDENCIES
        bml_test_core_support
)

# Sync Primitives Test
add_bml_test(SyncPrimitivesTest
        SOURCES
        SyncPrimitivesTest.cpp
        DEPENDENCIES
        bml_test_runtime_support
)

# SemanticVersion parsing and comparison tests
add_bml_test(SemanticVersionTests
        SOURCES
        SemanticVersionTests.cpp
        DEPENDENCIES
        bml_test_manifest_support
)

# SyncManager comprehensive tests
add_bml_test(SyncManagerTests
        SOURCES
        SyncManagerTests.cpp
        DEPENDENCIES
        bml_test_runtime_support
)

# ModuleLoader tests
add_bml_test(ModuleLoaderTests
        SOURCES
        ModuleLoaderTests.cpp
        DEPENDENCIES
        bml_test_module_loader_support
)

# MpscRingBuffer lock-free queue tests
add_bml_test(MpscRingBufferTests
        SOURCES
        MpscRingBufferTests.cpp
        DEPENDENCIES
        BML_Utils
)

# DiagnosticManager thread-local error handling tests
add_bml_test(DiagnosticManagerTests
        SOURCES
        DiagnosticManagerTests.cpp
        DEPENDENCIES
        bml_test_stub_logging_support
)

# TimerManager unit tests
add_bml_test(TimerManagerTests
        SOURCES
        TimerManagerTests.cpp
        DEPENDENCIES
        bml_test_runtime_support
)

# LocaleManager unit tests
add_bml_test(LocaleManagerTests
        SOURCES
        LocaleManagerTests.cpp
        DEPENDENCIES
        bml_test_runtime_support
)

# EventInterception unit tests
add_bml_test(EventInterceptionTests
        SOURCES
        EventInterceptionTests.cpp
        DEPENDENCIES
        bml_test_runtime_support
)

# FaultTracker unit tests
add_bml_test(FaultTrackerTests
        SOURCES
        FaultTrackerTests.cpp
        DEPENDENCIES
        bml_test_runtime_support
)

# HookRegistry unit tests
add_bml_test(HookRegistryTests
        SOURCES
        HookRegistryTests.cpp
        DEPENDENCIES
        bml_test_runtime_support
)

# RuntimeProvider registry unit tests
add_bml_test(RuntimeProviderTests
        SOURCES
        RuntimeProviderTests.cpp
        DEPENDENCIES
        bml_test_core_support
)

# AngelScript engine unit tests
if (TARGET angelscript)
    add_executable(ScriptingTests ScriptingTests.cpp)
    target_include_directories(ScriptingTests PRIVATE
            "${BML_INCLUDE_DIR}"
            "${BML_SOURCE_DIR}"
    )
    target_link_libraries(ScriptingTests PRIVATE
            gtest_main
            angelscript
            angelscript_addon
    )
    target_compile_definitions(ScriptingTests PRIVATE "BML_EXPORTS" "BML_TEST")
    set_target_properties(ScriptingTests PROPERTIES FOLDER "Tests")
    add_test(NAME ScriptingTests COMMAND $<TARGET_FILE:ScriptingTests>)

    add_bml_test(ScriptingRuntimeTests
            SOURCES
            ScriptingRuntimeTests.cpp
            ${CMAKE_SOURCE_DIR}/modules/BML_Scripting/src/BytecodeCache.cpp
            ${CMAKE_SOURCE_DIR}/modules/BML_Scripting/src/CoroutineManager.cpp
            ${CMAKE_SOURCE_DIR}/modules/BML_Scripting/src/ScriptEngine.cpp
            ${CMAKE_SOURCE_DIR}/modules/BML_Scripting/src/ScriptInstanceManager.cpp
            ${CMAKE_SOURCE_DIR}/modules/BML_Scripting/src/bindings/BindImc.cpp
            ${CMAKE_SOURCE_DIR}/modules/BML_Scripting/src/bindings/BindTimer.cpp
            DEPENDENCIES
            bml_test_runtime_support
            angelscript
            angelscript_addon
    )
    target_include_directories(ScriptingRuntimeTests PRIVATE
            ${CMAKE_SOURCE_DIR}/modules/BML_Scripting/src
    )
endif ()

# Integration tests linking against the real BML.dll
add_executable(BMLIntegrationTests BMLIntegrationTests.cpp)
target_include_directories(BMLIntegrationTests PRIVATE
        "${BML_INCLUDE_DIR}"
        "${BML_SOURCE_DIR}"
)
target_link_libraries(BMLIntegrationTests PRIVATE gtest_main BML)
set_target_properties(BMLIntegrationTests PROPERTIES
        FOLDER "Tests"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
add_test(NAME BMLIntegrationTests COMMAND $<TARGET_FILE:BMLIntegrationTests>)
add_dependencies(BMLIntegrationTests BML)

# Module framework + C++ wrapper integration tests (links real BML.dll)
add_executable(ModuleFrameworkIntegrationTests ModuleFrameworkIntegrationTests.cpp)
target_include_directories(ModuleFrameworkIntegrationTests PRIVATE
        "${BML_INCLUDE_DIR}"
        "${BML_SOURCE_DIR}"
)
target_link_libraries(ModuleFrameworkIntegrationTests PRIVATE gtest_main BML)
set_target_properties(ModuleFrameworkIntegrationTests PROPERTIES
        FOLDER "Tests"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
add_test(NAME ModuleFrameworkIntegrationTests COMMAND $<TARGET_FILE:ModuleFrameworkIntegrationTests>)
add_dependencies(ModuleFrameworkIntegrationTests BML)

# Test support infrastructure tests (links bml_test static library + real BML.dll)
add_executable(TestSupportTests TestSupportTests.cpp)
target_include_directories(TestSupportTests PRIVATE
        "${BML_INCLUDE_DIR}"
        "${BML_SOURCE_DIR}"
)
target_link_libraries(TestSupportTests PRIVATE gtest_main bml_test)
set_target_properties(TestSupportTests PROPERTIES
        FOLDER "Tests"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
add_test(NAME TestSupportTests COMMAND $<TARGET_FILE:TestSupportTests>)
add_dependencies(TestSupportTests bml_test)

# Built-in module API integration tests (real BML.dll + shared ANSI parser helpers)
add_executable(BuiltinModuleApiTests
        BuiltinModuleApiTests.cpp
        ${CMAKE_SOURCE_DIR}/modules/BML_Console/src/AnsiSegments.cpp
        ${CMAKE_SOURCE_DIR}/modules/BML_Console/src/AnsiPalette.cpp
        ${CMAKE_SOURCE_DIR}/modules/BML_Console/src/AnsiText.cpp
        ${IMGUI_SOURCE_DIR}/imgui.cpp
        ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
        ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
        ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
)
target_include_directories(BuiltinModuleApiTests PRIVATE
        "${BML_INCLUDE_DIR}"
        "${BML_SOURCE_DIR}"
        "${BML_SOURCE_DIR}/Utils"
        "${CMAKE_SOURCE_DIR}/modules/BML_Console/src"
        "${IMGUI_SOURCE_DIR}"
)
target_link_libraries(BuiltinModuleApiTests PRIVATE
        gtest_main
        BML_Utils
        BML_UI_api
)
target_compile_definitions(BuiltinModuleApiTests PRIVATE
        BML_EXPORTS
        BML_TEST
        IMGUI_EXPORT
)
set_target_properties(BuiltinModuleApiTests PROPERTIES
        FOLDER "Tests"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
add_test(NAME BuiltinModuleApiTests COMMAND $<TARGET_FILE:BuiltinModuleApiTests>)

add_bml_test(ConsoleCompletionStateTests
        SOURCES
        ConsoleCompletionStateTests.cpp
)

add_bml_test(MenuInputStateTests
        SOURCES
        MenuInputStateTests.cpp
)

add_test(
        NAME MigrationRepoCheck
        COMMAND ${CMAKE_COMMAND}
        -DREPO_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/cmake/CheckMigration.cmake
)

# InputRecorder format and engine tests (standalone, no BML core deps)
add_bml_test(InputRecorderTests
        SOURCES
        InputRecorderTests.cpp
        ${CMAKE_SOURCE_DIR}/modules/BML_InputRecorder/src/RecordingFormat.cpp
        ${CMAKE_SOURCE_DIR}/modules/BML_InputRecorder/src/RecorderEngine.cpp
)
target_include_directories(InputRecorderTests PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/BML_InputRecorder/src
)

# ProfilingManager trace events and Chrome Tracing tests
add_bml_test(ProfilingManagerTests
        SOURCES
        ProfilingManagerTests.cpp
        DEPENDENCIES
        bml_test_profiling_support
)
