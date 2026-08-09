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
                "Legacy native SDK consumer test needs ${required_variable}.")
    endif()
endforeach()

foreach(required_path
        CMAKE_EXECUTABLE
        SOURCE_ROOT
        MAIN_BUILD_DIR
        VIRTOOLS_SDK_PATH
        DUMPBIN)
    if(NOT EXISTS "${${required_path}}")
        message(FATAL_ERROR
                "Legacy native SDK consumer test path does not exist: "
                "${required_path}=${${required_path}}")
    endif()
endforeach()

get_filename_component(main_build_dir "${MAIN_BUILD_DIR}" ABSOLUTE)
get_filename_component(work_root "${WORK_ROOT}" ABSOLUTE)
file(TO_CMAKE_PATH "${main_build_dir}" main_build_dir)
file(TO_CMAKE_PATH "${work_root}" work_root)
set(main_build_prefix "${main_build_dir}/")
string(FIND "${work_root}/" "${main_build_prefix}" work_root_prefix)
if(NOT work_root_prefix EQUAL 0 OR work_root STREQUAL main_build_dir)
    message(FATAL_ERROR
            "Legacy native SDK consumer work root must be inside the main build "
            "directory: ${work_root}")
endif()

file(REMOVE_RECURSE "${work_root}")
set(install_root "${work_root}/install")
set(consumer_build_dir "${work_root}/build")

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

set(configure_command
        "${CMAKE_EXECUTABLE}"
        -S "${SOURCE_ROOT}/templates/native-mod-template"
        -B "${consumer_build_dir}"
        -G "${GENERATOR}")
if(DEFINED GENERATOR_PLATFORM AND NOT "${GENERATOR_PLATFORM}" STREQUAL "")
    list(APPEND configure_command -A "${GENERATOR_PLATFORM}")
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

message(STATUS
        "Verified the installed SDK with HelloMod.bmodp and its "
        "BMLEntry/BMLExit exports.")
