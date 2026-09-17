add_bml_test(ImcErrorTest
        SOURCES
        ImcErrorTest.cpp
        ${BML_SOURCE_DIR}/Api/Errors.cpp
)

add_bml_test(ImcHeadersTest
        SOURCES ImcHeadersTest.cpp
)
target_include_directories(ImcHeadersTest PRIVATE "${BML_IMC_SAMPLE_GENERATED_DIR}")
add_dependencies(ImcHeadersTest BML_ImcSampleApiGeneration)

add_bml_test(ImcRuntimeTest
        SOURCES
        ImcRuntimeTest.cpp
        ${BML_SOURCE_DIR}/Imc/ImcRuntime.cpp
)

add_bml_test(ImcWireTest
        SOURCES ImcWireTest.cpp
)

add_bml_test(ImcGeneratedClientTest
        SOURCES ImcGeneratedClientTest.cpp
)
target_include_directories(ImcGeneratedClientTest PRIVATE "${BML_IMC_SAMPLE_GENERATED_DIR}")
add_dependencies(ImcGeneratedClientTest BML_ImcSampleApiGeneration)

add_bml_test(ImcGeneratedRuntimeIntegrationTest
        SOURCES
        ImcGeneratedRuntimeIntegrationTest.cpp
        ${BML_SOURCE_DIR}/Imc/ImcRuntime.cpp
)
target_include_directories(ImcGeneratedRuntimeIntegrationTest PRIVATE "${BML_IMC_SAMPLE_GENERATED_DIR}")
add_dependencies(ImcGeneratedRuntimeIntegrationTest BML_ImcSampleApiGeneration)
set_tests_properties(ImcGeneratedRuntimeIntegrationTest PROPERTIES
        LABELS "imc;integration;performance")
