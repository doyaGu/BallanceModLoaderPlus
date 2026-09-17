# imc/test.sample.imc is the authoring input behind the generated header the
# generator tests compile against. The loader publishes no .imc interface of its
# own, so this sample is what keeps the generator, its lock format, and the
# committed output in step; smoke/smoke.native.imc covers the other half, a real
# Mod generating and using a binding at build time.
add_custom_target(BML_ImcSampleApiGeneration
        COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/imc_codegen.py"
                --check
                --out-dir "${BML_IMC_SAMPLE_GENERATED_DIR}"
                --input "${BML_IMC_SAMPLE_INPUT}"
        DEPENDS
                "${PROJECT_SOURCE_DIR}/tools/imc_codegen.py"
                "${BML_IMC_SAMPLE_INPUT}"
                "${BML_IMC_SAMPLE_INPUT}.lock"
        COMMENT "Verifying the generated test.sample IMC bindings"
)
set_target_properties(BML_ImcSampleApiGeneration PROPERTIES FOLDER "Tests")

# Keep the IMC generator and its compatibility rules executable.
add_test(
        NAME ImcCodegenTest
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/ImcCodegenTest.py"
                --source-root "${PROJECT_SOURCE_DIR}"
)
set_tests_properties(ImcCodegenTest PROPERTIES LABELS "imc;codegen")

add_test(
        NAME InterfaceCodegenTest
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/InterfaceCodegenTest.py"
                --source-root "${PROJECT_SOURCE_DIR}"
)
set_tests_properties(InterfaceCodegenTest PROPERTIES LABELS "interface;codegen")

add_test(
        NAME ImcCmakeIntegrationTest
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/ImcCmakeIntegrationTest.py"
                --source-root "${PROJECT_SOURCE_DIR}"
                --cmake "${CMAKE_COMMAND}"
                --generator "${CMAKE_GENERATOR}"
                --generator-platform "${CMAKE_GENERATOR_PLATFORM}"
)
set_tests_properties(ImcCmakeIntegrationTest PROPERTIES LABELS "imc;codegen;cmake")

add_test(
        NAME ImcCompatibilityIntegrationTest
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/ImcCompatibilityIntegrationTest.py"
                --source-root "${PROJECT_SOURCE_DIR}"
                --cmake "${CMAKE_COMMAND}"
                --generator "${CMAKE_GENERATOR}"
                --generator-platform "${CMAKE_GENERATOR_PLATFORM}"
)
set_tests_properties(ImcCompatibilityIntegrationTest PROPERTIES
        LABELS "imc;codegen;compatibility")
