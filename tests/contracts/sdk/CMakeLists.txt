add_test(
        NAME BmlCliTest
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/BmlCliTest.py"
                --source-root "${PROJECT_SOURCE_DIR}"
)
set_tests_properties(BmlCliTest PROPERTIES LABELS "sdk;workflow")

add_test(
        NAME ExecuteBBDepsTest
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/ExecuteBBDepsTest.py"
                --source-root "${PROJECT_SOURCE_DIR}"
)
set_tests_properties(ExecuteBBDepsTest PROPERTIES LABELS "architecture;legacy-native")

add_test(
        NAME BmlModCmakeTest
        COMMAND "${CMAKE_COMMAND}"
                "-DBML_MOD_CMAKE=${PROJECT_SOURCE_DIR}/cmake/BMLMod.cmake"
                -DTEST_CASE=compiler
                -P "${CMAKE_CURRENT_SOURCE_DIR}/BmlModCmakeTest.cmake"
)
set_tests_properties(BmlModCmakeTest PROPERTIES
        LABELS "legacy-native;cmake"
        PASS_REGULAR_EXPRESSION "bml_add_mod requires an MSVC-compatible C\\+\\+ ABI")

add_test(
        NAME BmlModArchitectureCmakeTest
        COMMAND "${CMAKE_COMMAND}"
                "-DBML_MOD_CMAKE=${PROJECT_SOURCE_DIR}/cmake/BMLMod.cmake"
                -DTEST_CASE=architecture
                -P "${CMAKE_CURRENT_SOURCE_DIR}/BmlModCmakeTest.cmake"
)
set_tests_properties(BmlModArchitectureCmakeTest PROPERTIES
        LABELS "legacy-native;cmake"
        PASS_REGULAR_EXPRESSION "bml_add_mod requires a 32-bit target")

add_test(
        NAME BmlModRuntimeLibraryCmakeTest
        COMMAND "${CMAKE_COMMAND}"
                "-DBML_MOD_CMAKE=${PROJECT_SOURCE_DIR}/cmake/BMLMod.cmake"
                -DTEST_CASE=runtime
                -P "${CMAKE_CURRENT_SOURCE_DIR}/BmlModCmakeTest.cmake"
)
set_tests_properties(BmlModRuntimeLibraryCmakeTest PROPERTIES
        LABELS "legacy-native;cmake;abi"
        PASS_REGULAR_EXPRESSION "bml_add_mod requires the MultiThreadedDLL MSVC runtime")

add_test(
        NAME BmlModRuntimePolicyCmakeTest
        COMMAND "${CMAKE_COMMAND}"
                "-DBML_MOD_CMAKE=${PROJECT_SOURCE_DIR}/cmake/BMLMod.cmake"
                -DTEST_CASE=runtime-policy
                -P "${CMAKE_CURRENT_SOURCE_DIR}/BmlModCmakeTest.cmake"
)
set_tests_properties(BmlModRuntimePolicyCmakeTest PROPERTIES
        LABELS "legacy-native;cmake;abi"
        PASS_REGULAR_EXPRESSION "bml_add_mod needs policy CMP0091 set to NEW")

if(MSVC OR CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    add_test(
            NAME BmlModRequiredExportsTest
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/BmlModRequiredExportsTest.py"
                    --source-root "${PROJECT_SOURCE_DIR}"
                    --cmake "${CMAKE_COMMAND}"
                    --generator "${CMAKE_GENERATOR}"
                    --generator-platform "${CMAKE_GENERATOR_PLATFORM}"
                    --cxx-compiler "${CMAKE_CXX_COMPILER}"
    )
    set_tests_properties(BmlModRequiredExportsTest PROPERTIES
            LABELS "legacy-native;cmake;integration"
            SKIP_REGULAR_EXPRESSION "BML_TEST_SKIPPED:"
            TIMEOUT 120)
endif()

if (MSVC AND EXISTS "${BML_DUMPBIN}")
    add_test(
            NAME LegacyNativeSdkConsumerTest
            COMMAND "${CMAKE_COMMAND}"
                    "-DCMAKE_EXECUTABLE=${CMAKE_COMMAND}"
                    "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
                    "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
                    "-DMAIN_BUILD_DIR=${PROJECT_BINARY_DIR}"
                    "-DWORK_ROOT=${PROJECT_BINARY_DIR}/tests/legacy-native-sdk-consumer-$<CONFIG>"
                    "-DGENERATOR=${CMAKE_GENERATOR}"
                    "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
                    "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
                    "-DCONFIGURATION=$<CONFIG>"
                    "-DVIRTOOLS_SDK_PATH=${VIRTOOLS_SDK_PATH}"
                    "-DDUMPBIN=${BML_DUMPBIN}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/LegacyNativeSdkConsumerTest.cmake"
    )
    set_tests_properties(LegacyNativeSdkConsumerTest PROPERTIES
            LABELS "legacy-native;integration;sdk"
            RUN_SERIAL TRUE
            SKIP_REGULAR_EXPRESSION "BML_TEST_SKIPPED:"
            TIMEOUT 180)

    add_test(
            NAME InterfacePackageIntegrationTest
            COMMAND "${CMAKE_COMMAND}"
                    "-DCMAKE_EXECUTABLE=${CMAKE_COMMAND}"
                    "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
                    "-DMAIN_BUILD_DIR=${PROJECT_BINARY_DIR}"
                    "-DWORK_ROOT=${PROJECT_BINARY_DIR}/tests/interface-package-$<CONFIG>"
                    "-DGENERATOR=${CMAKE_GENERATOR}"
                    "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
                    "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
                    "-DCONFIGURATION=$<CONFIG>"
                    "-DVIRTOOLS_SDK_PATH=${VIRTOOLS_SDK_PATH}"
                    "-DDUMPBIN=${BML_DUMPBIN}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/InterfacePackageIntegrationTest.cmake"
    )
    set_tests_properties(InterfacePackageIntegrationTest PROPERTIES
            LABELS "interface;cmake;integration;sdk"
            RUN_SERIAL TRUE
            SKIP_REGULAR_EXPRESSION "BML_TEST_SKIPPED:"
            TIMEOUT 180)

    add_test(
            NAME NativeModProfilesIntegrationTest
            COMMAND "${CMAKE_COMMAND}"
                    "-DCMAKE_EXECUTABLE=${CMAKE_COMMAND}"
                    "-DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}"
                    "-DMAIN_BUILD_DIR=${PROJECT_BINARY_DIR}"
                    "-DWORK_ROOT=${PROJECT_BINARY_DIR}/tests/native-mod-profiles-$<CONFIG>"
                    "-DGENERATOR=${CMAKE_GENERATOR}"
                    "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
                    "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
                    "-DCONFIGURATION=$<CONFIG>"
                    "-DVIRTOOLS_SDK_PATH=${VIRTOOLS_SDK_PATH}"
                    "-DDUMPBIN=${BML_DUMPBIN}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/NativeModProfilesIntegrationTest.cmake"
    )
    set_tests_properties(NativeModProfilesIntegrationTest PROPERTIES
            LABELS "legacy-native;interface;imc;integration;sdk"
            RUN_SERIAL TRUE
            SKIP_REGULAR_EXPRESSION "BML_TEST_SKIPPED:"
            TIMEOUT 240)
endif ()
