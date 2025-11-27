# Core sources
set(BML_CORE_SOURCES
        DllMain.cpp
        ApiRegistry.cpp
        CoreApi.cpp
        TracingApi.cpp
        CapabilityApi.cpp
        Context.cpp
        CoreErrors.cpp
        ConfigApi.cpp
        ConfigStore.cpp
        DependencyResolver.cpp
        DiagnosticManager.cpp
        DiagnosticApi.cpp
        Export.cpp
        ExtensionApi.cpp
        ResourceApi.cpp
        MemoryManager.cpp
        MemoryApi.cpp
        SyncManager.cpp
        SyncApi.cpp
        ProfilingManager.cpp
        ProfilingApi.cpp
        ModManifest.cpp
        ModuleDiscovery.cpp
        ModuleLoader.cpp
        ModuleRuntime.cpp
        HotReloadMonitor.cpp
        HotReloadWatchList.cpp
        Microkernel.cpp
        ImcBus.cpp
        ImcApi.cpp
        LoggingApi.cpp
        Logging.cpp
        SemanticVersion.cpp
)

set(BML_CORE_HEADERS
        ApiRegistry.h
        ApiRegistration.h
        Context.h
        CoreErrors.h
        ConfigStore.h
        DependencyResolver.h
        DiagnosticManager.h
        ResourceApi.h
        FixedBlockPool.h
        ImcBus.h
        Logging.h
        MemoryManager.h
        SyncManager.h
        ProfilingManager.h
        Microkernel.h
        ModHandle.h
        ModManifest.h
        ModuleDiscovery.h
        ModuleLoader.h
        ModuleRuntime.h
        MpscRingBuffer.h
        SpscRingBuffer.h
        SemanticVersion.h
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
        BMLUtils
        tomlplusplus::tomlplusplus
)

target_compile_definitions(BML PRIVATE
        BML_BUILD_DLL
)

set_target_properties(BML PROPERTIES
        OUTPUT_NAME "BML"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Core driver executable for testing
add_executable(BMLCoreDriver
        driver/CoreDriver.cpp
)

target_include_directories(BMLCoreDriver PRIVATE ${BML_INCLUDE_DIR})

target_link_libraries(BMLCoreDriver PRIVATE BML)

set_target_properties(BMLCoreDriver PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
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
