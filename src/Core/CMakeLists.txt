# Core sources
set(BML_CORE_SOURCES
        ApiRegistry.cpp
        RuntimeInterfaces.cpp
        CoreApi.cpp
        TracingApi.cpp
        Context.cpp
        CoreErrors.cpp
        ConfigApi.cpp
        ConfigStore.cpp
        DependencyResolver.cpp
        DiagnosticManager.cpp
        DiagnosticApi.cpp
        CrashDumpWriter.cpp
        Export.cpp
        FaultTracker.cpp
        HookRegistry.cpp
        HookApi.cpp
        ImcBusApi.cpp
        ImcBusFacade.cpp
        ImcBusPublish.cpp
        ImcBusRpc.cpp
        ImcBusState.cpp
        InterfaceApi.cpp
        ResourceApi.cpp
        TimerManager.cpp
        TimerApi.cpp
        LocaleManager.cpp
        LocaleApi.cpp
        MemoryManager.cpp
        MemoryApi.cpp
        ModManifest.cpp
        PackageInstaller.cpp
        PackageInstallState.cpp
        ModuleLifecycle.cpp
        Microkernel.cpp
        ImcBus.cpp
        KernelServices.cpp
        LoggingApi.cpp
        Logging.cpp
        SemanticVersion.cpp
        LeaseManager.cpp
        InterfaceRegistry.cpp
)

if (WIN32)
    list(APPEND BML_CORE_SOURCES
            DllMain.cpp
            SyncManager.cpp
            SyncApi.cpp
            ProfilingManager.cpp
            ProfilingApi.cpp
            ModuleDiscovery.cpp
            ModuleLoader.cpp
            ModuleRuntime.cpp
            FileSystemWatcher.cpp
            ReloadableModuleSlot.cpp
            HotReloadCoordinator.cpp
    )
else ()
    # Non-Windows: uses SyncManagerPosix; stubs live in stubs/ (see stubs/README.md)
    list(APPEND BML_CORE_SOURCES
            SyncManagerPosix.cpp
            SyncApi.cpp
            ProfilingManager.cpp
            ProfilingApi.cpp
            ModuleDiscovery.cpp
            ModuleLoader.cpp
            ModuleRuntime.cpp
            FileSystemWatcher.cpp
            ReloadableModuleSlot.cpp
            HotReloadCoordinator.cpp
    )
endif ()

set(BML_CORE_HEADERS
        ApiRegistry.h
        ApiRegistration.h
        RuntimeInterfaces.h
        Context.h
        CrashDumpWriter.h
        CoreErrors.h
        ConfigStore.h
        DependencyResolver.h
        DiagnosticManager.h
        FaultTracker.h
        ResourceApi.h
        FixedBlockPool.h
        HookRegistry.h
        ImcBus.h
        ImcBusInternal.h
        Logging.h
        MemoryManager.h
        SyncManager.h
        ProfilingManager.h
        Microkernel.h
        ModHandle.h
        ModManifest.h
        PackageInstaller.h
        PackageInstallState.h
        PackagePaths.h
        ModuleDiscovery.h
        ModuleLifecycle.h
        ModuleLoader.h
        ModuleRuntime.h
        MpscRingBuffer.h
        SpscRingBuffer.h
        SemanticVersion.h
        LeaseManager.h
        InterfaceRegistry.h
        KernelServices.h
        TimerManager.h
        LocaleManager.h
)

# Main BML Core DLL
add_library(BML SHARED
        ${BML_CORE_SOURCES}
        ${BML_CORE_HEADERS}
)

target_include_directories(BML
        PUBLIC
        $<BUILD_INTERFACE:${BML_INCLUDE_DIR}>
        $<BUILD_INTERFACE:${BML_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(BML
        PRIVATE
        BML_StringUtils
        BML_PathUtils
        BML_JsonUtils
        BML_TimeUtils
        BML_HashUtils
        zip
        tomlplusplus::tomlplusplus
        efsw
)

if (UNIX AND NOT APPLE)
    target_link_libraries(BML PRIVATE dl)
endif ()

target_compile_definitions(BML PRIVATE
        BML_BUILD_DLL
)

set_target_properties(BML PROPERTIES
        OUTPUT_NAME "BML"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Install targets
install(TARGETS BML
        EXPORT BMLCoreTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(EXPORT BMLCoreTargets
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/BMLCore"
        NAMESPACE BML::
)

# Standalone diagnostic driver (Windows only)
if (WIN32)
    add_executable(BMLCoreDriver
        CoreDriver.cpp
    )

    target_include_directories(BMLCoreDriver PRIVATE
        ${BML_INCLUDE_DIR}
        ${BML_SOURCE_DIR}
    )

    target_link_libraries(BMLCoreDriver PRIVATE
        BML
        BML_PathUtils
    )

    set_target_properties(BMLCoreDriver PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif ()
