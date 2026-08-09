foreach(required_variable
        CMAKE_EXECUTABLE
        SOURCE_ROOT
        WORK_ROOT
        GENERATOR
        CONFIGURATION
        VIRTOOLS_SDK_PATH
        DUMPBIN)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR
                "Legacy native SDK consumer test needs ${required_variable}.")
    endif()
endforeach()

set(use_sdk_archive FALSE)
if(DEFINED SDK_ARCHIVE AND NOT "${SDK_ARCHIVE}" STREQUAL "")
    set(use_sdk_archive TRUE)
elseif(NOT DEFINED MAIN_BUILD_DIR OR "${MAIN_BUILD_DIR}" STREQUAL "")
    message(FATAL_ERROR
            "Legacy native SDK consumer test needs MAIN_BUILD_DIR or SDK_ARCHIVE.")
endif()

foreach(required_path
        CMAKE_EXECUTABLE
        SOURCE_ROOT
        VIRTOOLS_SDK_PATH
        DUMPBIN)
    if(NOT EXISTS "${${required_path}}")
        message(FATAL_ERROR
                "Legacy native SDK consumer test path does not exist: "
                "${required_path}=${${required_path}}")
    endif()
endforeach()

if(use_sdk_archive)
    if(NOT EXISTS "${SDK_ARCHIVE}")
        message(FATAL_ERROR
                "Legacy native SDK consumer archive does not exist: ${SDK_ARCHIVE}")
    endif()
elseif(NOT EXISTS "${MAIN_BUILD_DIR}")
    message(FATAL_ERROR
            "Legacy native SDK consumer main build directory does not exist: "
            "${MAIN_BUILD_DIR}")
endif()

get_filename_component(source_root "${SOURCE_ROOT}" ABSOLUTE)
get_filename_component(work_root "${WORK_ROOT}" ABSOLUTE)
file(TO_CMAKE_PATH "${source_root}" source_root)
file(TO_CMAKE_PATH "${work_root}" work_root)
if(use_sdk_archive)
    set(work_root_guard "${source_root}")
else()
    get_filename_component(main_build_dir "${MAIN_BUILD_DIR}" ABSOLUTE)
    file(TO_CMAKE_PATH "${main_build_dir}" main_build_dir)
    set(work_root_guard "${main_build_dir}")
endif()
set(work_root_guard_prefix "${work_root_guard}/")
string(FIND "${work_root}/" "${work_root_guard_prefix}" work_root_prefix)
if(NOT work_root_prefix EQUAL 0 OR work_root STREQUAL work_root_guard)
    message(FATAL_ERROR
            "Legacy native SDK consumer work root must be inside the main build "
            "or source directory: ${work_root}")
endif()

file(REMOVE_RECURSE "${work_root}")
set(install_root "${work_root}/install")
set(consumer_build_dir "${work_root}/build")
set(consumer_modloader_dir "${work_root}/game/ModLoader")

if(use_sdk_archive)
    file(MAKE_DIRECTORY "${install_root}")
    execute_process(
        COMMAND "${CMAKE_EXECUTABLE}" -E tar xvf "${SDK_ARCHIVE}"
        WORKING_DIRECTORY "${install_root}"
        RESULT_VARIABLE extract_status
        OUTPUT_VARIABLE extract_output
        ERROR_VARIABLE extract_error
    )
    if(NOT extract_status EQUAL 0)
        message(FATAL_ERROR
                "Failed to extract the release SDK archive.\n"
                "${extract_output}${extract_error}")
    endif()
    set(consumer_source_dir "${install_root}/templates/native-mod-template")
else()
    execute_process(
        COMMAND "${CMAKE_EXECUTABLE}" --install "${main_build_dir}"
                --prefix "${install_root}" --config "${CONFIGURATION}"
        RESULT_VARIABLE install_status
        OUTPUT_VARIABLE install_output
        ERROR_VARIABLE install_error
    )
    if(NOT install_status EQUAL 0)
        message(FATAL_ERROR
                "Failed to install the SDK for its legacy native consumer.\n"
                "${install_output}${install_error}")
    endif()
    set(consumer_source_dir "${source_root}/templates/native-mod-template")
endif()

foreach(required_sdk_path
        "${install_root}/lib/cmake/BML/BMLConfig.cmake"
        "${install_root}/lib/BMLPlus.lib"
        "${consumer_source_dir}/CMakeLists.txt")
    if(NOT EXISTS "${required_sdk_path}")
        message(FATAL_ERROR
                "Legacy native SDK consumer input is incomplete: ${required_sdk_path}")
    endif()
endforeach()

set(configure_command
        "${CMAKE_EXECUTABLE}"
        -S "${consumer_source_dir}"
        -B "${consumer_build_dir}"
        -G "${GENERATOR}")
if(DEFINED GENERATOR_PLATFORM AND NOT "${GENERATOR_PLATFORM}" STREQUAL "")
    list(APPEND configure_command -A "${GENERATOR_PLATFORM}")
endif()
if(DEFINED CXX_COMPILER AND NOT "${CXX_COMPILER}" STREQUAL "")
    list(APPEND configure_command "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}")
endif()
list(APPEND configure_command
        "-DCMAKE_BUILD_TYPE=${CONFIGURATION}"
        "-DBML_DIR=${install_root}/lib/cmake/BML"
        "-DVIRTOOLS_SDK_PATH=${VIRTOOLS_SDK_PATH}"
        -DCMAKE_FIND_USE_PACKAGE_REGISTRY=FALSE
        -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=FALSE)

execute_process(
    COMMAND ${configure_command}
    RESULT_VARIABLE configure_status
    OUTPUT_VARIABLE configure_output
    ERROR_VARIABLE configure_error
)
if(NOT configure_status EQUAL 0)
    message(FATAL_ERROR
            "The installed SDK could not configure the legacy native Mod template.\n"
            "${configure_output}${configure_error}")
endif()

execute_process(
    COMMAND "${CMAKE_EXECUTABLE}" --build "${consumer_build_dir}"
            --config "${CONFIGURATION}"
    RESULT_VARIABLE build_status
    OUTPUT_VARIABLE build_output
    ERROR_VARIABLE build_error
)
if(NOT build_status EQUAL 0)
    message(FATAL_ERROR
            "The installed SDK could not build the legacy native Mod template.\n"
            "${build_output}${build_error}")
endif()

execute_process(
    COMMAND "${CMAKE_EXECUTABLE}" --install "${consumer_build_dir}"
            --prefix "${consumer_modloader_dir}" --config "${CONFIGURATION}"
    RESULT_VARIABLE consumer_install_status
    OUTPUT_VARIABLE consumer_install_output
    ERROR_VARIABLE consumer_install_error
)
if(NOT consumer_install_status EQUAL 0)
    message(FATAL_ERROR
            "The installed SDK could not install the native Mod template.\n"
            "${consumer_install_output}${consumer_install_error}")
endif()

set(installed_mod "${consumer_modloader_dir}/Mods/HelloMod.bmodp")
if(NOT EXISTS "${installed_mod}")
    message(FATAL_ERROR
            "The native Mod template install rule did not produce: ${installed_mod}")
endif()

file(GLOB_RECURSE mod_candidates LIST_DIRECTORIES FALSE
        "${consumer_build_dir}/HelloMod.bmodp"
        "${consumer_build_dir}/*/HelloMod.bmodp")
list(REMOVE_DUPLICATES mod_candidates)
list(LENGTH mod_candidates mod_count)
if(NOT mod_count EQUAL 1)
    message(FATAL_ERROR
            "Expected one HelloMod.bmodp from the installed SDK consumer build, "
            "found ${mod_count}: ${mod_candidates}")
endif()
list(GET mod_candidates 0 mod_binary)

execute_process(
    COMMAND "${DUMPBIN}" /exports "${mod_binary}"
    RESULT_VARIABLE dumpbin_status
    OUTPUT_VARIABLE exports
    ERROR_VARIABLE dumpbin_error
)
if(NOT dumpbin_status EQUAL 0)
    message(FATAL_ERROR "dumpbin /exports failed: ${dumpbin_error}")
endif()

set(found_entry FALSE)
set(found_exit FALSE)
string(REGEX MATCHALL "[^\r\n]+" export_lines "${exports}")
foreach(export_line IN LISTS export_lines)
    if(export_line MATCHES
            "^[ \t]*[0-9]+[ \t]+[0-9A-F]+[ \t]+[0-9A-F]+[ \t]+BMLEntry([ \t=]|$)")
        set(found_entry TRUE)
    elseif(export_line MATCHES
            "^[ \t]*[0-9]+[ \t]+[0-9A-F]+[ \t]+[0-9A-F]+[ \t]+BMLExit([ \t=]|$)")
        set(found_exit TRUE)
    endif()
endforeach()

if(NOT found_entry OR NOT found_exit)
    message(FATAL_ERROR
            "The native Mod template does not expose the required loader entry "
            "points. BMLEntry=${found_entry}, BMLExit=${found_exit}\n${exports}")
endif()

if(use_sdk_archive)
    find_program(powershell_executable NAMES pwsh powershell REQUIRED)
    set(script_template "${install_root}/templates/script-mod-template")
    set(script_packer "${install_root}/scripts/Pack-BMLScriptMod.ps1")
    set(script_project_module "${install_root}/scripts/lib/BMLProject.psm1")
    foreach(required_script_path
            "${script_template}/HelloScript.mod.as"
            "${script_template}/Resources/hello.txt"
            "${script_packer}"
            "${script_project_module}")
        if(NOT EXISTS "${required_script_path}")
            message(FATAL_ERROR
                    "The packaged SDK script consumer input is incomplete: "
                    "${required_script_path}")
        endif()
    endforeach()

    set(script_package_dir "${work_root}/script-package")
    set(script_package "${script_package_dir}/HelloScript.zip")
    file(MAKE_DIRECTORY "${script_package_dir}")
    execute_process(
        COMMAND "${powershell_executable}" -NoProfile -ExecutionPolicy Bypass
                -File "${script_packer}"
                -Source "${script_template}"
                -Output "${script_package}"
                -Force
        RESULT_VARIABLE script_pack_status
        OUTPUT_VARIABLE script_pack_output
        ERROR_VARIABLE script_pack_error
    )
    if(NOT script_pack_status EQUAL 0 OR NOT EXISTS "${script_package}")
        message(FATAL_ERROR
                "The packaged SDK could not package its script Mod template.\n"
                "${script_pack_output}${script_pack_error}")
    endif()

    execute_process(
        COMMAND "${CMAKE_EXECUTABLE}" -E tar tf "${script_package}"
        RESULT_VARIABLE script_list_status
        OUTPUT_VARIABLE script_package_entries_text
        ERROR_VARIABLE script_list_error
    )
    if(NOT script_list_status EQUAL 0)
        message(FATAL_ERROR
                "The packaged script Mod archive could not be inspected.\n"
                "${script_list_error}")
    endif()
    string(REPLACE "\r\n" "\n" script_package_entries_text "${script_package_entries_text}")
    string(REPLACE "\r" "\n" script_package_entries_text "${script_package_entries_text}")
    string(REPLACE "\n" ";" script_package_entries "${script_package_entries_text}")
    list(FILTER script_package_entries EXCLUDE REGEX "^$")

    foreach(required_entry
            "HelloScript.mod.as"
            "Resources/hello.txt")
        list(FIND script_package_entries "${required_entry}" required_entry_index)
        if(required_entry_index EQUAL -1)
            message(FATAL_ERROR
                    "The packaged script Mod is missing ${required_entry}: "
                    "${script_package_entries}")
        endif()
    endforeach()

    set(script_entry_count 0)
    foreach(package_entry IN LISTS script_package_entries)
        if(package_entry MATCHES "\\.mod\\.as$")
            math(EXPR script_entry_count "${script_entry_count} + 1")
        endif()
    endforeach()
    if(NOT script_entry_count EQUAL 1)
        message(FATAL_ERROR
                "Expected one script Mod entry in the packaged SDK consumer "
                "archive, found ${script_entry_count}: ${script_package_entries}")
    endif()
endif()

if(use_sdk_archive)
    set(sdk_input_label "SDK archive")
    set(script_validation_label " and packaged script Mod tooling")
else()
    set(sdk_input_label "installed SDK")
    set(script_validation_label "")
endif()
message(STATUS
        "Verified the ${sdk_input_label} with HelloMod.bmodp and its "
        "BMLEntry/BMLExit exports${script_validation_label}.")
