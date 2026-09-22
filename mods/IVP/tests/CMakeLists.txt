include(CTest)
if(IVP_BUILD_EVIDENCE_TESTS)
    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
endif()

include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.16.0)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

function(add_ivp_test test_name)
    add_executable(${test_name} ${ARGN} IvpTestAdapter.cpp)
    target_include_directories(${test_name} PRIVATE
            "${IVP_INCLUDE_DIR}"
            "${PROJECT_SOURCE_DIR}/src"
            "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(${test_name} PRIVATE BML::BML gtest_main gmock)
    target_compile_definitions(${test_name} PRIVATE
            BML_EXPORTS BML_TEST)
    target_compile_options(${test_name} PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/Zc:strictStrings->)
    set_target_properties(${test_name} PROPERTIES FOLDER "Tests/IVP")
    add_test(NAME ${test_name} COMMAND $<TARGET_FILE:${test_name}>)
    set_tests_properties(${test_name} PROPERTIES LABELS "ivp;host")
endfunction()

add_library(IvpAbiCompile OBJECT IvpAbiCompileTest.cpp)
add_library(IvpPackedConsumerCompile OBJECT IvpPackedConsumerCompile.cpp)
add_library(IvpByValueAbiCompile OBJECT IvpByValueAbiCompile.cpp)
foreach(target IN ITEMS IvpAbiCompile IvpPackedConsumerCompile IvpByValueAbiCompile)
    target_include_directories(${target} PRIVATE "${IVP_INCLUDE_DIR}")
    target_link_libraries(${target} PRIVATE
            BML::BML VirtoolsSDK::CK2 VirtoolsSDK::VxMath)
    target_compile_definitions(${target} PRIVATE BML_EXPORTS)
    set_target_properties(${target} PROPERTIES FOLDER "Tests/IVP")
endforeach()

foreach(default_call IN ITEMS Stdcall Fastcall)
    add_library(IvpCdecl${default_call}Compile OBJECT IvpCdeclCompile.c)
    target_include_directories(IvpCdecl${default_call}Compile PRIVATE
            "${IVP_INCLUDE_DIR}")
    target_compile_definitions(IvpCdecl${default_call}Compile PRIVATE
            BML_EXPORTS)
    target_link_libraries(IvpCdecl${default_call}Compile PRIVATE BML::BML)
    if(MSVC)
        if(default_call STREQUAL "Stdcall")
            target_compile_options(IvpCdecl${default_call}Compile PRIVATE /Gz /WX)
        else()
            target_compile_options(IvpCdecl${default_call}Compile PRIVATE /Gr /WX)
        endif()
    endif()
    set_target_properties(IvpCdecl${default_call}Compile PROPERTIES
            FOLDER "Tests/IVP")
endforeach()

set(IVP_HEADER_COMPILE_SOURCES)
foreach(header IN LISTS IVP_PUBLIC_RELATIVE_HEADERS)
    if(NOT header MATCHES "\\.h$")
        continue()
    endif()
    string(REPLACE "/" "_" stem "${header}")
    set(source "${CMAKE_CURRENT_BINARY_DIR}/public-headers/${stem}.cpp")
    file(WRITE "${source}" "#include <${header}>\n")
    list(APPEND IVP_HEADER_COMPILE_SOURCES "${source}")
endforeach()
add_library(IvpPublicHeadersCompile OBJECT ${IVP_HEADER_COMPILE_SOURCES})
target_include_directories(IvpPublicHeadersCompile PRIVATE
        "${IVP_INCLUDE_DIR}")
target_link_libraries(IvpPublicHeadersCompile PRIVATE
        BML::BML VirtoolsSDK::CK2 VirtoolsSDK::VxMath)
target_compile_definitions(IvpPublicHeadersCompile PRIVATE
        BML_EXPORTS)
set_target_properties(IvpPublicHeadersCompile PROPERTIES FOLDER "Tests/IVP")

if(IVP_BUILD_RETAIL_TESTS)
    add_executable(IvpRetailContactLifecycleTest
            IvpRetailOwnershipTest.cpp
            IvpRetailGeometryTest.cpp
            IvpRetailContactLifecycleTest.cpp
            IvpRetailUtilitiesTest.cpp
            IvpRetailTestSupport.cpp
            IvpTestAdapter.cpp)
    target_include_directories(IvpRetailContactLifecycleTest PRIVATE
            "${IVP_INCLUDE_DIR}"
            "${PROJECT_SOURCE_DIR}/src"
            "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(IvpRetailContactLifecycleTest PRIVATE
            BML::BML gtest_main gmock IVPCrypto)
    target_compile_definitions(IvpRetailContactLifecycleTest PRIVATE
            BML_EXPORTS BML_TEST)
    set_target_properties(IvpRetailContactLifecycleTest PROPERTIES FOLDER "Tests/IVP")
    add_test(NAME IvpRetailContactLifecycleTest
            COMMAND $<TARGET_FILE:IvpRetailContactLifecycleTest>)
    set_tests_properties(IvpRetailContactLifecycleTest PROPERTIES
            DISABLED TRUE LABELS "ivp;abi;retail;aggregate")

    file(TO_CMAKE_PATH
            "${IVP_BALLANCE_ROOT}/BuildingBlocks/physics_RT.dll"
            IVP_RETAIL_DLL)
    set(IVP_RETAIL_CASES
            "IvpRetailCrossDllOwnershipTest|IvpRetailCrossDllOwnership.*|ownership"
            "IvpRetailBroadphaseTest|IvpRetailBroadphase.*|broadphase"
            "IvpRetailByValueAbiTest|IvpRetailByValueAbi.*|abi"
            "IvpRetailCollisionDispatchTest|IvpRetailContactLifecycle.*|contact"
            "IvpRetailIncrementalLuTest|IvpRetailIncrementalLu.*|matrix"
            "IvpRetailContactGeometryTest|IvpRetailContactGeometry.*|geometry"
            "IvpRetailPublicUtilitiesTest|IvpRetailPublicUtilities.*|ownership"
            "IvpRetailControlValuesTest|IvpRetailControlValues.*|material"
            "IvpRetailPolygonTemplateTest|IvpRetailPolygonTemplate.*|geometry"
            "IvpRetailPointsoupBuilderTest|IvpRetailPointsoupBuilder.*|geometry")
    foreach(entry IN LISTS IVP_RETAIL_CASES)
        string(REPLACE "|" ";" parts "${entry}")
        list(GET parts 0 name)
        list(GET parts 1 filter)
        list(GET parts 2 label)
        add_test(NAME ${name}
                COMMAND $<TARGET_FILE:IvpRetailContactLifecycleTest>
                        "--gtest_filter=${filter}")
        set_tests_properties(${name} PROPERTIES
                LABELS "ivp;abi;retail;${label}"
                ENVIRONMENT "BML_TEST_RETAIL_PHYSICS_DLL=${IVP_RETAIL_DLL}")
    endforeach()
endif()

set(IVP_HOST_TESTS
        IvpContactMatrixTest
        IvpActiveValueGraphTest
        IvpPublicUtilityWorkflowTest
        IvpCrossDllOwnershipTest
        IvpBuoyancyInterpolationTest
        IvpCoreAttachmentLifecycleTest
        IvpCarGeometryTest
        IvpQuaternionKinematicsTest
        IvpConstraintConfigurationTest
        IvpObjectListenerLifecycleTest
        IvpObjectHierarchyTest
        IvpPhantomLifecycleTest
        IvpContactTopologyTest
        IvpRadarNeighborhoodTest
        IvpImmediateFreezeTest
        IvpCoreRecompileTest
        IvpObjectDeletionTest
        IvpBeamObjectTest
        IvpMovabilityTransitionTest
        IvpSimulationUnitSplitFuseTest
        IvpEnvironmentTimeRebaseTest
        IvpForceActuatorTest)
foreach(target IN LISTS IVP_HOST_TESTS)
    add_ivp_test(${target} ${target}.cpp)
endforeach()

add_executable(IvpInterfaceStructOffsetsTest IvpInterfaceStructOffsetsTest.cpp)
target_include_directories(IvpInterfaceStructOffsetsTest PRIVATE
        "${IVP_INCLUDE_DIR}")
target_link_libraries(IvpInterfaceStructOffsetsTest PRIVATE
        BML::BML gtest_main gmock)
target_compile_definitions(IvpInterfaceStructOffsetsTest PRIVATE
        BML_EXPORTS)
set_target_properties(IvpInterfaceStructOffsetsTest PROPERTIES
        FOLDER "Tests/IVP")
add_test(NAME IvpInterfaceStructOffsetsTest
        COMMAND $<TARGET_FILE:IvpInterfaceStructOffsetsTest>)
set_tests_properties(IvpInterfaceStructOffsetsTest PROPERTIES LABELS "ivp;host;abi")

if(IVP_BUILD_EVIDENCE_TESTS)
    add_test(NAME IvpRetailContractTest
            COMMAND ${Python3_EXECUTABLE}
                    "${PROJECT_SOURCE_DIR}/tools/ivp/sync_retail_contract.py" --check)
    add_test(NAME IvpIdbCorrectionStagesTest
            COMMAND ${Python3_EXECUTABLE}
                    "${CMAKE_CURRENT_SOURCE_DIR}/IvpIdbCorrectionStagesTest.py")
    add_test(NAME IvpHeaderCompletionSitesTest
            COMMAND ${Python3_EXECUTABLE}
                    "${CMAKE_CURRENT_SOURCE_DIR}/IvpHeaderCompletionSitesTest.py")
    add_test(NAME IvpPublicAuditDefinitionTest
            COMMAND ${Python3_EXECUTABLE}
                    "${CMAKE_CURRENT_SOURCE_DIR}/IvpPublicAuditDefinitionTest.py")
    add_test(NAME IvpBitfieldAbiAuditTest
            COMMAND ${Python3_EXECUTABLE}
                    "${PROJECT_SOURCE_DIR}/tools/ivp/Audit-IvpBitfieldAbi.py"
                    --source-root "${PROJECT_SOURCE_DIR}"
                    --bml-include-root "${IVP_BML_INCLUDE_DIR}")
    add_test(NAME IvpVtableSurfaceAbiAuditTest
            COMMAND ${Python3_EXECUTABLE}
                    "${PROJECT_SOURCE_DIR}/tools/ivp/Audit-IvpVtableSurfaceAbi.py"
                    --source-root "${PROJECT_SOURCE_DIR}"
                    --bml-include-root "${IVP_BML_INCLUDE_DIR}")

    foreach(test IN ITEMS
            IvpRetailContractTest
            IvpIdbCorrectionStagesTest
            IvpHeaderCompletionSitesTest
            IvpPublicAuditDefinitionTest
            IvpBitfieldAbiAuditTest
            IvpVtableSurfaceAbiAuditTest)
        set_tests_properties(${test} PROPERTIES LABELS "ivp;abi;audit")
    endforeach()
endif()

if(MSVC)
    get_filename_component(IVP_TOOL_DIRECTORY "${CMAKE_LINKER}" DIRECTORY)
    set(IVP_DUMPBIN "${IVP_TOOL_DIRECTORY}/dumpbin.exe")
    if(EXISTS "${IVP_DUMPBIN}")
        add_test(
                NAME IvpModExportAbiTest
                COMMAND "${CMAKE_COMMAND}"
                        "-DDUMPBIN=${IVP_DUMPBIN}"
                        "-DBINARY=$<TARGET_FILE:IVP>"
                        "-P" "${CMAKE_CURRENT_SOURCE_DIR}/IvpModExportAbiTest.cmake")
        set_tests_properties(IvpModExportAbiTest PROPERTIES
                LABELS "ivp;abi;boundary")

        add_test(
                NAME IvpSdkConsumerTest
                COMMAND "${CMAKE_COMMAND}"
                        "-DCMAKE_EXECUTABLE=${CMAKE_COMMAND}"
                        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
                        "-DMAIN_BUILD_DIR=${CMAKE_BINARY_DIR}"
                        "-DWORK_ROOT=${CMAKE_CURRENT_BINARY_DIR}/sdk-consumer-$<CONFIG>"
                        "-DGENERATOR=${CMAKE_GENERATOR}"
                        "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
                        "-DCONFIG=$<CONFIG>"
                        "-DBML_DIR=${BML_DIR}"
                        "-DDUMPBIN=${IVP_DUMPBIN}"
                        "-P" "${CMAKE_CURRENT_SOURCE_DIR}/IvpSdkConsumerTest.cmake")
        set_tests_properties(IvpSdkConsumerTest PROPERTIES
                LABELS "ivp;abi;boundary;cmake;integration"
                TIMEOUT 180)
    endif()
endif()

if(IVP_BUILD_EVIDENCE_TESTS AND EXISTS "${IVP_REFERENCE_ROOT}")
    execute_process(
            COMMAND "${Python3_EXECUTABLE}" -c
                    "import struct; print(struct.calcsize('P') * 8)"
            OUTPUT_VARIABLE IVP_AUDIT_PYTHON_BITS
            OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
if(IVP_BUILD_EVIDENCE_TESTS AND EXISTS "${IVP_REFERENCE_ROOT}"
        AND IVP_AUDIT_PYTHON_BITS STREQUAL "64")
    add_test(NAME IvpPublicFieldAuditTest
            COMMAND "${Python3_EXECUTABLE}"
                    "${PROJECT_SOURCE_DIR}/tools/ivp/Audit-IvpPublicFields.py"
                    --reference-root "${IVP_REFERENCE_ROOT}"
                    --format summary-tsv --check)
    add_test(NAME IvpPublicFreeFunctionAuditTest
            COMMAND "${Python3_EXECUTABLE}"
                    "${PROJECT_SOURCE_DIR}/tools/ivp/Audit-IvpPublicFreeFunctions.py"
                    --reference-root "${IVP_REFERENCE_ROOT}"
                    --format summary-tsv --check)
    add_test(NAME IvpByValueSurfaceAuditTest
            COMMAND "${Python3_EXECUTABLE}"
                    "${PROJECT_SOURCE_DIR}/tools/ivp/Audit-IvpByValueAbi.py"
                    --reference-root "${IVP_REFERENCE_ROOT}")
    foreach(test IN ITEMS IvpPublicFieldAuditTest IvpPublicFreeFunctionAuditTest
                          IvpByValueSurfaceAuditTest)
        set_tests_properties(${test} PROPERTIES
                LABELS "ivp;abi;audit" TIMEOUT 180)
    endforeach()
endif()

if(IVP_BUILD_PLAYER_TESTS)
    add_subdirectory(player)
endif()
