add_executable(ImcBenchmark
        ImcBenchmark.cpp
        ${CMAKE_SOURCE_DIR}/tests/ModuleLoaderStub.cpp
        ${CMAKE_SOURCE_DIR}/tests/TestKernelSupport.cpp
        ${BML_SOURCE_DIR}/Core/ImcBus.cpp
        ${BML_SOURCE_DIR}/Core/ImcBusApi.cpp
        ${BML_SOURCE_DIR}/Core/ImcBusFacade.cpp
        ${BML_SOURCE_DIR}/Core/ImcBusPublish.cpp
        ${BML_SOURCE_DIR}/Core/ImcBusRpc.cpp
        ${BML_SOURCE_DIR}/Core/ImcBusState.cpp
        ${BML_SOURCE_DIR}/Core/CrashDumpWriter.cpp
        ${BML_SOURCE_DIR}/Core/FaultTracker.cpp
        ${BML_SOURCE_DIR}/Core/HookRegistry.cpp
        ${BML_SOURCE_DIR}/Core/ApiRegistry.cpp
        ${BML_SOURCE_DIR}/Core/CoreErrors.cpp
        ${BML_SOURCE_DIR}/Core/DiagnosticManager.cpp
        ${BML_SOURCE_DIR}/Core/Logging.cpp
        ${BML_SOURCE_DIR}/Core/Context.cpp
        ${BML_SOURCE_DIR}/Core/ConfigStore.cpp
        ${BML_SOURCE_DIR}/Core/ResourceApi.cpp
)

set_target_properties(ImcBenchmark PROPERTIES
        FOLDER "Benchmarks"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_include_directories(ImcBenchmark
        PRIVATE
        ${BML_INCLUDE_DIR}
        ${BML_SOURCE_DIR}
)

target_link_libraries(ImcBenchmark
        PRIVATE
        BML_Utils
        tomlplusplus::tomlplusplus
)

target_compile_definitions(ImcBenchmark PRIVATE "BML_EXPORTS")
