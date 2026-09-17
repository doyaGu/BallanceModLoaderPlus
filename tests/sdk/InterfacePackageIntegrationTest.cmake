foreach(required_variable
        CMAKE_EXECUTABLE
        SOURCE_ROOT
        MAIN_BUILD_DIR
        WORK_ROOT
        GENERATOR
        CONFIGURATION
        VIRTOOLS_SDK_PATH
        DUMPBIN)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR
                "Interface package integration test needs ${required_variable}.")
    endif()
endforeach()

if(NOT GENERATOR MATCHES "^Visual Studio" AND "$ENV{INCLUDE}" STREQUAL "")
    message(STATUS
            "BML_TEST_SKIPPED: the ${GENERATOR} generator needs an MSVC "
            "developer environment; INCLUDE is not set.")
    return()
endif()

foreach(required_path
        CMAKE_EXECUTABLE
        SOURCE_ROOT
        MAIN_BUILD_DIR
        VIRTOOLS_SDK_PATH
        DUMPBIN)
    if(NOT EXISTS "${${required_path}}")
        message(FATAL_ERROR
                "Interface package integration path does not exist: "
                "${required_path}=${${required_path}}")
    endif()
endforeach()

get_filename_component(source_root "${SOURCE_ROOT}" ABSOLUTE)
get_filename_component(main_build_dir "${MAIN_BUILD_DIR}" ABSOLUTE)
get_filename_component(work_root "${WORK_ROOT}" ABSOLUTE)
file(TO_CMAKE_PATH "${source_root}" source_root)
file(TO_CMAKE_PATH "${main_build_dir}" main_build_dir)
file(TO_CMAKE_PATH "${work_root}" work_root)

set(work_root_guard_prefix "${main_build_dir}/")
string(FIND "${work_root}/" "${work_root_guard_prefix}" work_root_prefix)
if(NOT work_root_prefix EQUAL 0 OR work_root STREQUAL main_build_dir)
    message(FATAL_ERROR
            "Interface package integration work root must be inside the main build: ${work_root}")
endif()

file(REMOVE_RECURSE "${work_root}")
set(bml_sdk "${work_root}/bml-sdk")
set(provider_build "${work_root}/provider-build")
set(provider_package "${work_root}/provider-package")
set(consumer_build "${work_root}/consumer-build")
set(consumer_package "${work_root}/consumer-package")

execute_process(
    COMMAND "${CMAKE_EXECUTABLE}" --install "${main_build_dir}"
            --prefix "${bml_sdk}" --config "${CONFIGURATION}"
    RESULT_VARIABLE bml_install_status
    OUTPUT_VARIABLE bml_install_output
    ERROR_VARIABLE bml_install_error
)
if(NOT bml_install_status EQUAL 0)
    message(FATAL_ERROR
            "Failed to install the BML SDK for the interface package test.\n"
            "${bml_install_output}${bml_install_error}")
endif()

set(provider_source "${bml_sdk}/examples/native-interface-provider")
set(consumer_source "${bml_sdk}/examples/native-interface-consumer")
foreach(required_sdk_path
        "${bml_sdk}/lib/cmake/BML/BMLInterface.cmake"
        "${bml_sdk}/lib/cmake/BML/BMLInterfacePackageConfig.cmake.in"
        "${provider_source}/CMakeLists.txt"
        "${provider_source}/include/BMLExample/ValueInterface.h"
        "${consumer_source}/CMakeLists.txt")
    if(NOT EXISTS "${required_sdk_path}")
        message(FATAL_ERROR
                "Installed BML SDK is missing an interface package input: ${required_sdk_path}")
    endif()
endforeach()

function(configure_example source_dir build_dir install_dir)
    set(configure_command
            "${CMAKE_EXECUTABLE}"
            -S "${source_dir}"
            -B "${build_dir}"
            -G "${GENERATOR}")
    if(DEFINED GENERATOR_PLATFORM AND NOT "${GENERATOR_PLATFORM}" STREQUAL "")
        list(APPEND configure_command -A "${GENERATOR_PLATFORM}")
    endif()
    if(DEFINED CXX_COMPILER AND NOT "${CXX_COMPILER}" STREQUAL "")
        list(APPEND configure_command "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}")
    endif()
    list(APPEND configure_command
            "-DCMAKE_BUILD_TYPE=${CONFIGURATION}"
            "-DCMAKE_INSTALL_PREFIX=${install_dir}"
            "-DBML_DIR=${bml_sdk}/lib/cmake/BML"
            "-DVIRTOOLS_SDK_PATH=${VIRTOOLS_SDK_PATH}"
            -DCMAKE_FIND_USE_PACKAGE_REGISTRY=FALSE
            -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=FALSE)
    if(ARGC GREATER 3)
        list(APPEND configure_command "-DBMLExampleValue_DIR=${ARGV3}")
    endif()

    execute_process(
        COMMAND ${configure_command}
        RESULT_VARIABLE configure_status
        OUTPUT_VARIABLE configure_output
        ERROR_VARIABLE configure_error
    )
    if(NOT configure_status EQUAL 0)
        message(FATAL_ERROR
                "Interface example configure failed for ${source_dir}.\n"
                "${configure_output}${configure_error}")
    endif()
endfunction()

configure_example("${provider_source}" "${provider_build}" "${provider_package}")
execute_process(
    COMMAND "${CMAKE_EXECUTABLE}" --build "${provider_build}"
            --config "${CONFIGURATION}" --target install
    RESULT_VARIABLE provider_build_status
    OUTPUT_VARIABLE provider_build_output
    ERROR_VARIABLE provider_build_error
)
if(NOT provider_build_status EQUAL 0)
    message(FATAL_ERROR
            "Provider interface package build/install failed.\n"
            "${provider_build_output}${provider_build_error}")
endif()

set(interface_package_dir "${provider_package}/lib/cmake/BMLExampleValue")
foreach(required_provider_output
        "${provider_package}/Mods/BMLExampleValueProvider.bmodp"
        "${provider_package}/include/BMLExample/ValueInterface.h"
        "${interface_package_dir}/BMLExampleValueConfig.cmake"
        "${interface_package_dir}/BMLExampleValueConfigVersion.cmake"
        "${interface_package_dir}/BMLExampleValueTargets.cmake")
    if(NOT EXISTS "${required_provider_output}")
        message(FATAL_ERROR
                "Provider package is missing: ${required_provider_output}")
    endif()
endforeach()

file(READ "${interface_package_dir}/BMLExampleValueTargets.cmake" exported_targets)
string(FIND "${exported_targets}" "BMLExampleValue::Interface" exported_target_index)
if(exported_target_index EQUAL -1)
    message(FATAL_ERROR
            "Provider package did not export BMLExampleValue::Interface.")
endif()
string(FIND "${exported_targets}" "${source_root}" source_leak_index)
if(NOT source_leak_index EQUAL -1)
    message(FATAL_ERROR
            "Provider interface target leaked the BML source tree into its installed package.")
endif()
string(FIND "${exported_targets}" "${provider_source}" provider_source_leak_index)
if(NOT provider_source_leak_index EQUAL -1)
    message(FATAL_ERROR
            "Provider interface target leaked its build-tree include path into the installed package.")
endif()

configure_example("${consumer_source}" "${consumer_build}" "${consumer_package}"
                  "${interface_package_dir}")
execute_process(
    COMMAND "${CMAKE_EXECUTABLE}" --build "${consumer_build}"
            --config "${CONFIGURATION}" --target install
    RESULT_VARIABLE consumer_build_status
    OUTPUT_VARIABLE consumer_build_output
    ERROR_VARIABLE consumer_build_error
)
if(NOT consumer_build_status EQUAL 0)
    message(FATAL_ERROR
            "Independent interface consumer build/install failed.\n"
            "${consumer_build_output}${consumer_build_error}")
endif()

set(consumer_mod "${consumer_package}/Mods/BMLExampleValueConsumer.bmodp")
if(NOT EXISTS "${consumer_mod}")
    message(FATAL_ERROR "Independent interface consumer is missing: ${consumer_mod}")
endif()

execute_process(
    COMMAND "${DUMPBIN}" /dependents "${consumer_mod}"
    RESULT_VARIABLE dependents_status
    OUTPUT_VARIABLE dependents
    ERROR_VARIABLE dependents_error
)
if(NOT dependents_status EQUAL 0)
    message(FATAL_ERROR "dumpbin /dependents failed: ${dependents_error}")
endif()
string(TOLOWER "${dependents}" dependents_lower)
string(FIND "${dependents_lower}" "bmlexamplevalueprovider" provider_dependency_index)
if(NOT provider_dependency_index EQUAL -1)
    message(FATAL_ERROR
            "Consumer binary depends on the provider binary; it must link only the interface package.")
endif()
string(FIND "${dependents_lower}" "bmlplus.dll" loader_dependency_index)
if(loader_dependency_index EQUAL -1)
    message(FATAL_ERROR
            "Consumer binary does not import the BML loader as expected: ${dependents}")
endif()

execute_process(
    COMMAND "${DUMPBIN}" /imports "${consumer_mod}"
    RESULT_VARIABLE imports_status
    OUTPUT_VARIABLE imports
    ERROR_VARIABLE imports_error
)
if(NOT imports_status EQUAL 0)
    message(FATAL_ERROR "dumpbin /imports failed: ${imports_error}")
endif()
string(TOLOWER "${imports}" imports_lower)
string(FIND "${imports_lower}" "bmlexamplevalueprovider" provider_import_index)
if(NOT provider_import_index EQUAL -1)
    message(FATAL_ERROR
            "Consumer import table contains a provider import: ${imports}")
endif()

message(STATUS
        "Independent provider and consumer packages built; consumer imports no provider binary.")
