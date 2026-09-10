foreach(required_variable
        CMAKE_EXECUTABLE
        PYTHON_EXECUTABLE
        MAIN_BUILD_DIR
        WORK_ROOT
        GENERATOR
        CONFIGURATION
        VIRTOOLS_SDK_PATH
        DUMPBIN)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR
                "Native Mod profiles integration test needs ${required_variable}.")
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
        PYTHON_EXECUTABLE
        MAIN_BUILD_DIR
        VIRTOOLS_SDK_PATH
        DUMPBIN)
    if(NOT EXISTS "${${required_path}}")
        message(FATAL_ERROR
                "Native Mod profiles integration path does not exist: "
                "${required_path}=${${required_path}}")
    endif()
endforeach()

get_filename_component(main_build_dir "${MAIN_BUILD_DIR}" ABSOLUTE)
get_filename_component(work_root "${WORK_ROOT}" ABSOLUTE)
file(TO_CMAKE_PATH "${main_build_dir}" main_build_dir)
file(TO_CMAKE_PATH "${work_root}" work_root)

set(work_root_guard_prefix "${main_build_dir}/")
string(FIND "${work_root}/" "${work_root_guard_prefix}" work_root_prefix)
if(NOT work_root_prefix EQUAL 0 OR work_root STREQUAL main_build_dir)
    message(FATAL_ERROR
            "Native Mod profiles work root must be inside the main build: ${work_root}")
endif()

file(REMOVE_RECURSE "${work_root}")
set(bml_sdk "${work_root}/bml-sdk")
execute_process(
    COMMAND "${CMAKE_EXECUTABLE}" --install "${main_build_dir}"
            --prefix "${bml_sdk}" --config "${CONFIGURATION}"
    RESULT_VARIABLE install_status
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error
)
if(NOT install_status EQUAL 0)
    message(FATAL_ERROR
            "Failed to install the BML SDK for the profile test.\n"
            "${install_output}${install_error}")
endif()

set(workflow "${bml_sdk}/scripts/bml.py")
set(workflow_launcher "${bml_sdk}/scripts/bml.cmd")
foreach(required_sdk_path
        "${workflow}"
        "${workflow_launcher}"
        "${bml_sdk}/share/BML/tools/interface_codegen.py"
        "${bml_sdk}/templates/native-mod-template/CMakeLists.txt"
        "${bml_sdk}/templates/native-interface-provider-template/CMakeLists.txt"
        "${bml_sdk}/templates/native-interface-consumer-template/CMakeLists.txt"
        "${bml_sdk}/templates/native-imc-provider-template/CMakeLists.txt"
        "${bml_sdk}/templates/native-imc-provider-template/api/service.imc.lock")
    if(NOT EXISTS "${required_sdk_path}")
        message(FATAL_ERROR "Installed SDK is missing a profile input: ${required_sdk_path}")
    endif()
endforeach()
if(EXISTS "${bml_sdk}/scripts/bml.ps1" OR
   EXISTS "${bml_sdk}/scripts/New-BMLNativeMod.ps1" OR
   EXISTS "${bml_sdk}/scripts/New-BMLScriptMod.ps1" OR
   EXISTS "${bml_sdk}/scripts/Pack-BMLScriptMod.ps1")
    message(FATAL_ERROR "Installed SDK still contains the removed PowerShell workflow.")
endif()

# Adopting the Python workflow in an existing CMake Mod must only add local
# tool metadata. It must not rewrite the author's CMake or source files.
set(existing_source "${work_root}/existing-source")
file(MAKE_DIRECTORY "${existing_source}/src")
file(READ "${bml_sdk}/templates/native-mod-template/CMakeLists.txt" existing_cmake)
string(REPLACE "HelloMod" "ExistingMod" existing_cmake "${existing_cmake}")
string(REPLACE "1.0.0" "2.3.4" existing_cmake "${existing_cmake}")
file(WRITE "${existing_source}/CMakeLists.txt" "${existing_cmake}")
file(READ "${bml_sdk}/templates/native-mod-template/src/HelloMod.cpp" existing_cpp)
string(REPLACE "HelloMod" "ExistingMod" existing_cpp "${existing_cpp}")
string(REPLACE "1.0.0" "2.3.4" existing_cpp "${existing_cpp}")
file(WRITE "${existing_source}/src/ExistingMod.cpp" "${existing_cpp}")
file(SHA256 "${existing_source}/CMakeLists.txt" existing_cmake_before)
file(SHA256 "${existing_source}/src/ExistingMod.cpp" existing_source_before)
execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" "${workflow}" init "test.existing"
            --project "${existing_source}"
    RESULT_VARIABLE init_status
    OUTPUT_VARIABLE init_output
    ERROR_VARIABLE init_error
)
if(NOT init_status EQUAL 0 OR
   NOT EXISTS "${existing_source}/bml.mod.json" OR
   NOT EXISTS "${existing_source}/bml.py" OR
   NOT EXISTS "${existing_source}/bml.cmd" OR
   EXISTS "${existing_source}/bml.ps1")
    message(FATAL_ERROR
            "bml.py init failed.\n${init_output}${init_error}")
endif()
file(SHA256 "${existing_source}/CMakeLists.txt" existing_cmake_after)
file(SHA256 "${existing_source}/src/ExistingMod.cpp" existing_source_after)
if(NOT existing_cmake_before STREQUAL existing_cmake_after OR
   NOT existing_source_before STREQUAL existing_source_after)
    message(FATAL_ERROR "bml.py init changed existing author-owned files.")
endif()
file(READ "${existing_source}/bml.mod.json" existing_manifest)
if(NOT existing_manifest MATCHES "\"target\": \"ExistingMod\"" OR
   NOT existing_manifest MATCHES "\"version\": \"2.3.4\"" OR
   NOT existing_manifest MATCHES "\"kind\": \"native\"")
    message(FATAL_ERROR "bml.py init did not infer the existing project metadata.")
endif()
execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" "${existing_source}/bml.py" build
            --project "${existing_source}"
            --virtools-sdk "${VIRTOOLS_SDK_PATH}"
            --configuration "${CONFIGURATION}"
    RESULT_VARIABLE existing_build_status
    OUTPUT_VARIABLE existing_build_output
    ERROR_VARIABLE existing_build_error
)
if(NOT existing_build_status EQUAL 0 OR
   NOT EXISTS "${existing_source}/.bml/stage/Mods/ExistingMod.bmodp")
    message(FATAL_ERROR
            "Initialized existing Mod did not build.\n"
            "${existing_build_output}${existing_build_error}")
endif()
execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" "${existing_source}/bml.py" pack
            --project "${existing_source}"
            --virtools-sdk "${VIRTOOLS_SDK_PATH}"
            --configuration "${CONFIGURATION}"
    RESULT_VARIABLE existing_pack_status
    OUTPUT_VARIABLE existing_pack_output
    ERROR_VARIABLE existing_pack_error
)
if(NOT existing_pack_status EQUAL 0 OR
   NOT EXISTS "${existing_source}/dist/ExistingMod.bmodp")
    message(FATAL_ERROR
            "Initialized existing Mod did not produce its publishable artifact.\n"
            "${existing_pack_output}${existing_pack_error}")
endif()

function(scaffold_profile profile source_dir mod_id mod_name)
    set(command
            "${PYTHON_EXECUTABLE}" "${workflow}" new native "${mod_id}"
            --profile "${profile}"
            --name "${mod_name}"
            --author "SDK Profile Test"
            --destination "${source_dir}")
    if(ARGC GREATER 4)
        list(APPEND command --provider-id "${ARGV4}")
    endif()
    execute_process(
        COMMAND ${command}
        RESULT_VARIABLE scaffold_status
        OUTPUT_VARIABLE scaffold_output
        ERROR_VARIABLE scaffold_error
    )
    if(NOT scaffold_status EQUAL 0)
        message(FATAL_ERROR
                "Could not scaffold ${profile}.\n${scaffold_output}${scaffold_error}")
    endif()

    file(GLOB_RECURSE generated_text
            "${source_dir}/*.cpp"
            "${source_dir}/*.h"
            "${source_dir}/*.hpp"
            "${source_dir}/*.txt"
            "${source_dir}/*.md"
            "${source_dir}/*.imc"
            "${source_dir}/*.bml-interface"
            "${source_dir}/*.lock")
    foreach(generated_file IN LISTS generated_text)
        file(READ "${generated_file}" generated_contents)
        if(generated_contents MATCHES "__[A-Z][A-Z0-9_]*__")
            message(FATAL_ERROR
                    "${profile} left an unresolved token in ${generated_file}.")
        endif()
    endforeach()
    if(NOT EXISTS "${source_dir}/bml.mod.json" OR
       NOT EXISTS "${source_dir}/.gitignore" OR
       NOT EXISTS "${source_dir}/bml.cmd" OR
       NOT EXISTS "${source_dir}/bml.py" OR
       NOT EXISTS "${source_dir}/.bml/settings.json")
        message(FATAL_ERROR
                "${profile} did not create its manifest, local workflow, and settings.")
    endif()
    if(EXISTS "${source_dir}/bml.ps1")
        message(FATAL_ERROR "${profile} generated the removed PowerShell workflow.")
    endif()
endfunction()

function(configure_profile source_dir build_dir install_dir)
    set(command
            "${CMAKE_EXECUTABLE}"
            -S "${source_dir}"
            -B "${build_dir}"
            -G "${GENERATOR}")
    if(DEFINED GENERATOR_PLATFORM AND NOT "${GENERATOR_PLATFORM}" STREQUAL "")
        list(APPEND command -A "${GENERATOR_PLATFORM}")
    endif()
    if(DEFINED CXX_COMPILER AND NOT "${CXX_COMPILER}" STREQUAL "")
        list(APPEND command "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}")
    endif()
    list(APPEND command
            "-DCMAKE_BUILD_TYPE=${CONFIGURATION}"
            "-DCMAKE_INSTALL_PREFIX=${install_dir}"
            "-DBML_DIR=${bml_sdk}/lib/cmake/BML"
            "-DVIRTOOLS_SDK_PATH=${VIRTOOLS_SDK_PATH}"
            -DCMAKE_FIND_USE_PACKAGE_REGISTRY=FALSE
            -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=FALSE)
    if(ARGC GREATER 3)
        list(APPEND command "-DValueProviderModInterface_DIR=${ARGV3}")
    endif()
    execute_process(
        COMMAND ${command}
        RESULT_VARIABLE configure_status
        OUTPUT_VARIABLE configure_output
        ERROR_VARIABLE configure_error
    )
    if(NOT configure_status EQUAL 0)
        message(FATAL_ERROR
                "Could not configure generated project ${source_dir}.\n"
                "${configure_output}${configure_error}")
    endif()
endfunction()

function(build_profile build_dir profile)
    execute_process(
        COMMAND "${CMAKE_EXECUTABLE}" --build "${build_dir}"
                --config "${CONFIGURATION}" --target install
        RESULT_VARIABLE build_status
        OUTPUT_VARIABLE build_output
        ERROR_VARIABLE build_error
    )
    if(NOT build_status EQUAL 0)
        message(FATAL_ERROR
                "Could not build/install generated ${profile}.\n"
                "${build_output}${build_error}")
    endif()
endfunction()

set(basic_source "${work_root}/basic-source")
set(provider_source "${work_root}/provider-source")
set(consumer_source "${work_root}/consumer-source")
set(imc_source "${work_root}/imc-source")
scaffold_profile(basic "${basic_source}" "test.basic" "Basic Profile")
scaffold_profile(interface-provider "${provider_source}"
                 "test.value-provider" "Value Provider")
scaffold_profile(interface-consumer "${consumer_source}"
                 "test.value-consumer" "Value Consumer" "test.value-provider")
scaffold_profile(imc-provider "${imc_source}" "test.remote-api" "Remote API")

# Exercise the public two-command path, including inferred name/author, manifest
# discovery, generator selection input, Win32 configuration, build, and staging.
set(cli_source "${work_root}/cli-source")
execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" "${workflow}" new native "test.one-command"
            --destination "${cli_source}"
    RESULT_VARIABLE cli_new_status
    OUTPUT_VARIABLE cli_new_output
    ERROR_VARIABLE cli_new_error
)
if(NOT cli_new_status EQUAL 0 OR NOT EXISTS "${cli_source}/bml.mod.json")
    message(FATAL_ERROR
            "bml.py new failed.\n${cli_new_output}${cli_new_error}")
endif()
execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" "${cli_source}/bml.py" build
            --project "${cli_source}"
            --virtools-sdk "${VIRTOOLS_SDK_PATH}"
            --configuration "${CONFIGURATION}"
    RESULT_VARIABLE cli_dev_status
    OUTPUT_VARIABLE cli_dev_output
    ERROR_VARIABLE cli_dev_error
)
if(NOT cli_dev_status EQUAL 0 OR
   NOT EXISTS "${cli_source}/.bml/stage/Mods/OneCommandMod.bmodp" OR
   NOT EXISTS "${cli_source}/.bml/settings.json")
    message(FATAL_ERROR
            "bml.py build failed.\n${cli_dev_output}${cli_dev_error}")
endif()
execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" "${provider_source}/bml.py" interface check
            --project "${provider_source}"
    RESULT_VARIABLE interface_check_status
    OUTPUT_VARIABLE interface_check_output
    ERROR_VARIABLE interface_check_error
)
if(NOT interface_check_status EQUAL 0 OR
   NOT EXISTS "${provider_source}/.bml/interface-preview/ValueProviderMod/ValueInterface.h")
    message(FATAL_ERROR
            "bml.py interface check failed.\n"
            "${interface_check_output}${interface_check_error}")
endif()

file(READ "${basic_source}/src/BasicMod.cpp" basic_source_text)
string(FIND "${basic_source_text}" "BML_UnregisterCommand(\"hello\")" unregister_index)
if(unregister_index EQUAL -1)
    message(FATAL_ERROR "The basic profile does not unregister its owned command.")
endif()

set(basic_install "${work_root}/basic-install")
set(provider_install "${work_root}/provider-install")
set(consumer_install "${work_root}/consumer-install")
set(imc_install "${work_root}/imc-install")
configure_profile("${basic_source}" "${work_root}/basic-build" "${basic_install}")
build_profile("${work_root}/basic-build" basic)
configure_profile("${provider_source}" "${work_root}/provider-build" "${provider_install}")
build_profile("${work_root}/provider-build" interface-provider)

set(interface_package_dir
        "${provider_install}/lib/cmake/ValueProviderModInterface")
configure_profile("${consumer_source}" "${work_root}/consumer-build"
                  "${consumer_install}" "${interface_package_dir}")
build_profile("${work_root}/consumer-build" interface-consumer)
configure_profile("${imc_source}" "${work_root}/imc-build" "${imc_install}")
build_profile("${work_root}/imc-build" imc-provider)

set(basic_mod "${basic_install}/Mods/BasicMod.bmodp")
set(provider_mod "${provider_install}/Mods/ValueProviderMod.bmodp")
set(consumer_mod "${consumer_install}/Mods/ValueConsumerMod.bmodp")
set(imc_mod "${imc_install}/Mods/RemoteApiMod.bmodp")
foreach(expected_output
        "${basic_mod}"
        "${provider_mod}"
        "${consumer_mod}"
        "${imc_mod}"
        "${provider_install}/include/ValueProviderMod/ValueInterface.h"
        "${interface_package_dir}/ValueProviderModInterfaceConfig.cmake"
        "${work_root}/imc-build/bml-imc/test_remoteapi_api_imc.hpp")
    if(NOT EXISTS "${expected_output}")
        message(FATAL_ERROR "Generated profile output is missing: ${expected_output}")
    endif()
endforeach()

foreach(mod_binary IN ITEMS "${basic_mod}" "${provider_mod}" "${consumer_mod}" "${imc_mod}")
    execute_process(
        COMMAND "${DUMPBIN}" /exports "${mod_binary}"
        RESULT_VARIABLE exports_status
        OUTPUT_VARIABLE exports
        ERROR_VARIABLE exports_error
    )
    if(NOT exports_status EQUAL 0 OR
       NOT exports MATCHES "BMLEntry" OR NOT exports MATCHES "BMLExit")
        message(FATAL_ERROR
                "Generated profile has invalid exports: ${mod_binary}\n"
                "${exports}${exports_error}")
    endif()
endforeach()

execute_process(
    COMMAND "${DUMPBIN}" /dependents "${consumer_mod}"
    RESULT_VARIABLE dependents_status
    OUTPUT_VARIABLE dependents
    ERROR_VARIABLE dependents_error
)
string(TOLOWER "${dependents}" dependents_lower)
if(NOT dependents_status EQUAL 0 OR
   dependents_lower MATCHES "valueprovidermod")
    message(FATAL_ERROR
            "Generated consumer links the provider binary.\n"
            "${dependents}${dependents_error}")
endif()

message(STATUS
        "All native Mod profiles scaffolded from empty directories and built successfully.")
