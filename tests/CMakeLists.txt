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

add_bml_test(IniFileTest
        SOURCES
        IniFileTest.cpp
        DEPENDENCIES
        BMLUtils
)

add_bml_test(AnsiPaletteTest
        SOURCES
        AnsiPaletteTest.cpp
        ${BML_SOURCE_DIR}/AnsiPalette.cpp
        DEPENDENCIES
        BMLUtils
)
# Need ImGui headers for AnsiPalette
target_include_directories(AnsiPaletteTest PRIVATE ${IMGUI_SOURCE_DIR})

add_bml_test(ManifestParserTest
        SOURCES
        ManifestParserTest.cpp
        ${BML_SOURCE_DIR}/Core/ModManifest.cpp
        ${BML_SOURCE_DIR}/Core/SemanticVersion.cpp
        DEPENDENCIES
        BMLUtils tomlplusplus::tomlplusplus
)

add_bml_test(ModuleDiscoveryTest
        SOURCES
        ModuleDiscoveryTest.cpp
        ${BML_SOURCE_DIR}/Core/ModuleDiscovery.cpp
        ${BML_SOURCE_DIR}/Core/ModManifest.cpp
        ${BML_SOURCE_DIR}/Core/DependencyResolver.cpp
        ${BML_SOURCE_DIR}/Core/SemanticVersion.cpp
        DEPENDENCIES
        BMLUtils tomlplusplus::tomlplusplus
)

add_bml_test(DependencyResolverTest
        SOURCES
        DependencyResolverTest.cpp
        ${BML_SOURCE_DIR}/Core/DependencyResolver.cpp
        ${BML_SOURCE_DIR}/Core/SemanticVersion.cpp
        DEPENDENCIES
        BMLUtils
)

add_bml_test(ImcBusTest
        SOURCES
        ImcBusTest.cpp
        ModuleLoaderStub.cpp
        ${BML_SOURCE_DIR}/Core/ImcBus.cpp
        ${BML_SOURCE_DIR}/Core/ApiRegistry.cpp
        ${BML_SOURCE_DIR}/Core/CoreErrors.cpp
        ${BML_SOURCE_DIR}/Core/DiagnosticManager.cpp
        ${BML_SOURCE_DIR}/Core/Logging.cpp
        ${BML_SOURCE_DIR}/Core/Context.cpp
        ${BML_SOURCE_DIR}/Core/ConfigStore.cpp
        ${BML_SOURCE_DIR}/Core/ResourceApi.cpp
        DEPENDENCIES
        BMLUtils
        tomlplusplus::tomlplusplus
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
        ModuleLoaderStub.cpp
        ${BML_SOURCE_DIR}/Core/CoreApi.cpp
        ${BML_SOURCE_DIR}/Core/ApiRegistry.cpp
        ${BML_SOURCE_DIR}/Core/ConfigApi.cpp
        ${BML_SOURCE_DIR}/Core/ConfigStore.cpp
        ${BML_SOURCE_DIR}/Core/Context.cpp
        ${BML_SOURCE_DIR}/Core/CoreErrors.cpp
        ${BML_SOURCE_DIR}/Core/DiagnosticManager.cpp
        ${BML_SOURCE_DIR}/Core/ImcApi.cpp
        ${BML_SOURCE_DIR}/Core/ImcBus.cpp
        ${BML_SOURCE_DIR}/Core/Logging.cpp
        ${BML_SOURCE_DIR}/Core/LoggingApi.cpp
        ${BML_SOURCE_DIR}/Core/ResourceApi.cpp
        DEPENDENCIES
        BMLUtils
        tomlplusplus::tomlplusplus
)

add_bml_test(ApiRegistryTests
        SOURCES
        ApiRegistryTests.cpp
        ModuleLoaderStub.cpp
        ${BML_SOURCE_DIR}/Core/ApiRegistry.cpp
        ${BML_SOURCE_DIR}/Core/ConfigStore.cpp
        ${BML_SOURCE_DIR}/Core/Context.cpp
        ${BML_SOURCE_DIR}/Core/CoreErrors.cpp
        ${BML_SOURCE_DIR}/Core/DiagnosticManager.cpp
        ${BML_SOURCE_DIR}/Core/Logging.cpp
        ${BML_SOURCE_DIR}/Core/ResourceApi.cpp
        DEPENDENCIES
        BMLUtils
        tomlplusplus::tomlplusplus
)

add_bml_test(ContextLifecycleTests
        SOURCES
        ContextLifecycleTests.cpp
        ModuleLoaderStub.cpp
        ${BML_SOURCE_DIR}/Core/ApiRegistry.cpp
        ${BML_SOURCE_DIR}/Core/ConfigStore.cpp
        ${BML_SOURCE_DIR}/Core/Context.cpp
        ${BML_SOURCE_DIR}/Core/CoreErrors.cpp
        ${BML_SOURCE_DIR}/Core/DiagnosticManager.cpp
        ${BML_SOURCE_DIR}/Core/Logging.cpp
        ${BML_SOURCE_DIR}/Core/ResourceApi.cpp
        DEPENDENCIES
        BMLUtils
        tomlplusplus::tomlplusplus
)

add_bml_test(ConfigStoreConcurrencyTests
        SOURCES
        ConfigStoreConcurrencyTests.cpp
        ModuleLoaderStub.cpp
        ${BML_SOURCE_DIR}/Core/ApiRegistry.cpp
        ${BML_SOURCE_DIR}/Core/ConfigApi.cpp
        ${BML_SOURCE_DIR}/Core/ConfigStore.cpp
        ${BML_SOURCE_DIR}/Core/Context.cpp
        ${BML_SOURCE_DIR}/Core/CoreErrors.cpp
        ${BML_SOURCE_DIR}/Core/DiagnosticManager.cpp
        ${BML_SOURCE_DIR}/Core/Logging.cpp
        ${BML_SOURCE_DIR}/Core/ResourceApi.cpp
        DEPENDENCIES
        BMLUtils
        tomlplusplus::tomlplusplus
)

add_bml_test(ResourceHandleLifecycleTests
        SOURCES
        ResourceHandleLifecycleTests.cpp
        ModuleLoaderStub.cpp
        ${BML_SOURCE_DIR}/Core/ApiRegistry.cpp
        ${BML_SOURCE_DIR}/Core/Context.cpp
        ${BML_SOURCE_DIR}/Core/CoreErrors.cpp
        ${BML_SOURCE_DIR}/Core/DiagnosticManager.cpp
        ${BML_SOURCE_DIR}/Core/ConfigStore.cpp
        ${BML_SOURCE_DIR}/Core/Logging.cpp
        ${BML_SOURCE_DIR}/Core/ResourceApi.cpp
        DEPENDENCIES
        BMLUtils
        tomlplusplus::tomlplusplus
)

add_bml_test(MemoryManagerTests
        SOURCES
        MemoryManagerTests.cpp
        TestLoggingStub.cpp
        ${BML_SOURCE_DIR}/Core/CoreErrors.cpp
        ${BML_SOURCE_DIR}/Core/DiagnosticManager.cpp
        ${BML_SOURCE_DIR}/Core/MemoryManager.cpp
        DEPENDENCIES
        BMLUtils
)

add_bml_test(ExtensionApiValidationTests
        SOURCES
        ExtensionApiValidationTests.cpp
        ModuleLoaderStub.cpp
        ${BML_SOURCE_DIR}/Core/ApiRegistry.cpp
        ${BML_SOURCE_DIR}/Core/ConfigStore.cpp
        ${BML_SOURCE_DIR}/Core/Context.cpp
        ${BML_SOURCE_DIR}/Core/CoreErrors.cpp
        ${BML_SOURCE_DIR}/Core/DiagnosticManager.cpp
        ${BML_SOURCE_DIR}/Core/ExtensionApi.cpp
        ${BML_SOURCE_DIR}/Core/Logging.cpp
        ${BML_SOURCE_DIR}/Core/ResourceApi.cpp
        DEPENDENCIES
        BMLUtils
        tomlplusplus::tomlplusplus
)

add_bml_test(LoggingSinkOverrideTests
        SOURCES
        LoggingSinkOverrideTests.cpp
        ModuleLoaderStub.cpp
        ${BML_SOURCE_DIR}/Core/ApiRegistry.cpp
        ${BML_SOURCE_DIR}/Core/ConfigStore.cpp
        ${BML_SOURCE_DIR}/Core/Context.cpp
        ${BML_SOURCE_DIR}/Core/CoreErrors.cpp
        ${BML_SOURCE_DIR}/Core/DiagnosticManager.cpp
        ${BML_SOURCE_DIR}/Core/Logging.cpp
        ${BML_SOURCE_DIR}/Core/LoggingApi.cpp
        ${BML_SOURCE_DIR}/Core/ResourceApi.cpp
        DEPENDENCIES
        BMLUtils
        tomlplusplus::tomlplusplus
)

add_bml_test(CoreErrorsGuardTests
        SOURCES
        CoreErrorsGuardTests.cpp
        ModuleLoaderStub.cpp
        ${BML_SOURCE_DIR}/Core/ApiRegistry.cpp
        ${BML_SOURCE_DIR}/Core/ConfigStore.cpp
        ${BML_SOURCE_DIR}/Core/Context.cpp
        ${BML_SOURCE_DIR}/Core/CoreErrors.cpp
        ${BML_SOURCE_DIR}/Core/DiagnosticManager.cpp
        ${BML_SOURCE_DIR}/Core/Logging.cpp
        ${BML_SOURCE_DIR}/Core/LoggingApi.cpp
        ${BML_SOURCE_DIR}/Core/ResourceApi.cpp
        DEPENDENCIES
        BMLUtils
        tomlplusplus::tomlplusplus
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
        ${BML_SOURCE_DIR}/Core/HotReloadMonitor.cpp
        ${BML_SOURCE_DIR}/Core/HotReloadWatchList.cpp
        ${BML_SOURCE_DIR}/Core/ImcBus.cpp
        ${BML_SOURCE_DIR}/Core/Logging.cpp
        ${BML_SOURCE_DIR}/Core/ModuleDiscovery.cpp
        ${BML_SOURCE_DIR}/Core/ModuleRuntime.cpp
        ${BML_SOURCE_DIR}/Core/ModManifest.cpp
        ${BML_SOURCE_DIR}/Core/SemanticVersion.cpp
        DEPENDENCIES
        BMLUtils
        tomlplusplus::tomlplusplus
)

add_dependencies(HotReloadIntegrationTests HotReloadSampleMod)
target_compile_definitions(HotReloadIntegrationTests PRIVATE
        BML_TEST_SAMPLE_MOD_DLL="$<TARGET_FILE:HotReloadSampleMod>")
]]

# API Improvements Test (v0.5.0 capability system, discovery, performance)
add_bml_test(ApiImprovementsTest
        SOURCES
        ApiImprovementsTest.cpp
        ModuleLoaderStub.cpp
        ${BML_SOURCE_DIR}/Core/ApiRegistry.cpp
        ${BML_SOURCE_DIR}/Core/ConfigStore.cpp
        ${BML_SOURCE_DIR}/Core/Context.cpp
        ${BML_SOURCE_DIR}/Core/CoreErrors.cpp
        ${BML_SOURCE_DIR}/Core/DiagnosticManager.cpp
        ${BML_SOURCE_DIR}/Core/Logging.cpp
        ${BML_SOURCE_DIR}/Core/ResourceApi.cpp
        DEPENDENCIES
        BMLUtils
        tomlplusplus::tomlplusplus
)

# Sync Primitives Test
add_bml_test(SyncPrimitivesTest
        SOURCES
        SyncPrimitivesTest.cpp
        ModuleLoaderStub.cpp
        ${BML_SOURCE_DIR}/Core/ApiRegistry.cpp
        ${BML_SOURCE_DIR}/Core/ConfigStore.cpp
        ${BML_SOURCE_DIR}/Core/Context.cpp
        ${BML_SOURCE_DIR}/Core/CoreErrors.cpp
        ${BML_SOURCE_DIR}/Core/DiagnosticManager.cpp
        ${BML_SOURCE_DIR}/Core/Logging.cpp
        ${BML_SOURCE_DIR}/Core/SyncManager.cpp
        ${BML_SOURCE_DIR}/Core/ResourceApi.cpp
        DEPENDENCIES
        BMLUtils
        tomlplusplus::tomlplusplus
)

# SemanticVersion parsing and comparison tests
add_bml_test(SemanticVersionTests
        SOURCES
        SemanticVersionTests.cpp
        ${BML_SOURCE_DIR}/Core/SemanticVersion.cpp
        DEPENDENCIES
        BMLUtils
)

# SyncManager comprehensive tests
add_bml_test(SyncManagerTests
        SOURCES
        SyncManagerTests.cpp
        ModuleLoaderStub.cpp
        ${BML_SOURCE_DIR}/Core/SyncManager.cpp
        ${BML_SOURCE_DIR}/Core/ApiRegistry.cpp
        ${BML_SOURCE_DIR}/Core/ConfigStore.cpp
        ${BML_SOURCE_DIR}/Core/Context.cpp
        ${BML_SOURCE_DIR}/Core/CoreErrors.cpp
        ${BML_SOURCE_DIR}/Core/DiagnosticManager.cpp
        ${BML_SOURCE_DIR}/Core/ResourceApi.cpp
        ${BML_SOURCE_DIR}/Core/Logging.cpp
        DEPENDENCIES
        BMLUtils
        tomlplusplus::tomlplusplus
)

# ModuleLoader tests
add_bml_test(ModuleLoaderTests
        SOURCES
        ModuleLoaderTests.cpp
        ModuleLoaderStub.cpp
        ${BML_SOURCE_DIR}/Core/Context.cpp
        ${BML_SOURCE_DIR}/Core/CoreErrors.cpp
        ${BML_SOURCE_DIR}/Core/DiagnosticManager.cpp
        ${BML_SOURCE_DIR}/Core/DependencyResolver.cpp
        ${BML_SOURCE_DIR}/Core/SemanticVersion.cpp
        ${BML_SOURCE_DIR}/Core/Logging.cpp
        ${BML_SOURCE_DIR}/Core/ApiRegistry.cpp
        ${BML_SOURCE_DIR}/Core/ConfigStore.cpp
        ${BML_SOURCE_DIR}/Core/ResourceApi.cpp
        DEPENDENCIES
        BMLUtils
        tomlplusplus::tomlplusplus
)

# MpscRingBuffer lock-free queue tests
add_bml_test(MpscRingBufferTests
        SOURCES
        MpscRingBufferTests.cpp
        DEPENDENCIES
        BMLUtils
)

# DiagnosticManager thread-local error handling tests
add_bml_test(DiagnosticManagerTests
        SOURCES
        DiagnosticManagerTests.cpp
        ${BML_SOURCE_DIR}/Core/DiagnosticManager.cpp
        DEPENDENCIES
        BMLUtils
)

# ProfilingManager trace events and Chrome Tracing tests
add_bml_test(ProfilingManagerTests
        SOURCES
        ProfilingManagerTests.cpp
        ModuleLoaderStub.cpp
        ${BML_SOURCE_DIR}/Core/ProfilingManager.cpp
        ${BML_SOURCE_DIR}/Core/DiagnosticManager.cpp
        ${BML_SOURCE_DIR}/Core/MemoryManager.cpp
        ${BML_SOURCE_DIR}/Core/CoreErrors.cpp
        ${BML_SOURCE_DIR}/Core/ApiRegistry.cpp
        ${BML_SOURCE_DIR}/Core/ConfigStore.cpp
        ${BML_SOURCE_DIR}/Core/Context.cpp
        ${BML_SOURCE_DIR}/Core/ResourceApi.cpp
        ${BML_SOURCE_DIR}/Core/Logging.cpp
        DEPENDENCIES
        BMLUtils
        tomlplusplus::tomlplusplus
)
