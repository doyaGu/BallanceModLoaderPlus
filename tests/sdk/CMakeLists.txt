cmake_minimum_required(VERSION 3.25)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(BMLSdkTests LANGUAGES NONE)
    get_filename_component(PROJECT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
    enable_testing()
    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
endif()


add_test(
        NAME BmlCliTest
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/BmlCliTest.py"
                --source-root "${PROJECT_SOURCE_DIR}"
)
set_tests_properties(BmlCliTest PROPERTIES LABELS "sdk;workflow")

add_test(
        NAME BmlSdlCliTest
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/BmlSdlCliTest.py"
                --source-root "${PROJECT_SOURCE_DIR}"
)
set_tests_properties(BmlSdlCliTest PROPERTIES LABELS "sdk;workflow;sdl")

if(CK2_INSTALL AND VXMATH_INSTALL)
    add_test(
            NAME SdlSdkConsumerTest
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/SdlSdkConsumerTest.py"
                    --main-build-dir "${PROJECT_BINARY_DIR}"
                    --work-root "${PROJECT_BINARY_DIR}/tests/sdl-sdk-consumer"
                    --cmake "${CMAKE_COMMAND}"
                    --configuration "$<CONFIG>"
    )
    set_tests_properties(SdlSdkConsumerTest PROPERTIES LABELS "sdk;workflow;sdl;integration"
            RUN_SERIAL TRUE TIMEOUT 240)
endif()

add_test(
        NAME NativeModResourcePackageTest
        COMMAND "${CMAKE_COMMAND}"
                "-DWORK_ROOT=${PROJECT_BINARY_DIR}/tests/native-mod-resource-package"
                "-DGENERATOR=${CMAKE_GENERATOR}"
                "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/NativeModResourcePackageTest.cmake"
)
set_tests_properties(NativeModResourcePackageTest PROPERTIES
        LABELS "sdk;packaging;integration"
        TIMEOUT 120)

add_test(
        NAME ExecuteBBDepsTest
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/ExecuteBBDepsTest.py"
                --source-root "${PROJECT_SOURCE_DIR}"
)
set_tests_properties(ExecuteBBDepsTest PROPERTIES LABELS "architecture;native")

add_test(
        NAME BmlModCmakeTest
        COMMAND "${CMAKE_COMMAND}"
                "-DBML_MOD_CMAKE=${PROJECT_SOURCE_DIR}/cmake/BMLMod.cmake"
                -DTEST_CASE=compiler
                -P "${CMAKE_CURRENT_SOURCE_DIR}/BmlModCmakeTest.cmake"
)
set_tests_properties(BmlModCmakeTest PROPERTIES
        LABELS "native;cmake"
        PASS_REGULAR_EXPRESSION "This BML SDK requires")

add_test(
        NAME BmlModArchitectureCmakeTest
        COMMAND "${CMAKE_COMMAND}"
                "-DBML_MOD_CMAKE=${PROJECT_SOURCE_DIR}/cmake/BMLMod.cmake"
                -DTEST_CASE=architecture
                -P "${CMAKE_CURRENT_SOURCE_DIR}/BmlModCmakeTest.cmake"
)
set_tests_properties(BmlModArchitectureCmakeTest PROPERTIES
        LABELS "native;cmake"
        PASS_REGULAR_EXPRESSION "The Mod target does not match this BML SDK platform/word size")

add_test(
        NAME BmlModSdlArchitectureCmakeTest
        COMMAND "${CMAKE_COMMAND}"
                "-DBML_MOD_CMAKE=${PROJECT_SOURCE_DIR}/cmake/BMLMod.cmake"
                -DTEST_CASE=sdl-architecture
                -P "${CMAKE_CURRENT_SOURCE_DIR}/BmlModCmakeTest.cmake"
)
set_tests_properties(BmlModSdlArchitectureCmakeTest PROPERTIES
        LABELS "sdk;sdl;cmake;abi"
        PASS_REGULAR_EXPRESSION "The Mod architecture x64 does not match this BML SDK")

add_test(
        NAME BmlModRuntimeLibraryCmakeTest
        COMMAND "${CMAKE_COMMAND}"
                "-DBML_MOD_CMAKE=${PROJECT_SOURCE_DIR}/cmake/BMLMod.cmake"
                -DTEST_CASE=runtime
                -P "${CMAKE_CURRENT_SOURCE_DIR}/BmlModCmakeTest.cmake"
)
set_tests_properties(BmlModRuntimeLibraryCmakeTest PROPERTIES
        LABELS "native;cmake;abi"
        PASS_REGULAR_EXPRESSION "bml_add_mod requires the MultiThreadedDLL MSVC runtime")

add_test(
        NAME BmlModRuntimePolicyCmakeTest
        COMMAND "${CMAKE_COMMAND}"
                "-DBML_MOD_CMAKE=${PROJECT_SOURCE_DIR}/cmake/BMLMod.cmake"
                -DTEST_CASE=runtime-policy
                -P "${CMAKE_CURRENT_SOURCE_DIR}/BmlModCmakeTest.cmake"
)
set_tests_properties(BmlModRuntimePolicyCmakeTest PROPERTIES
        LABELS "native;cmake;abi"
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
            LABELS "native;cmake;integration"
            SKIP_REGULAR_EXPRESSION "BML_TEST_SKIPPED:"
            TIMEOUT 120)
endif()
