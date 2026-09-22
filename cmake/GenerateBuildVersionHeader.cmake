# GenerateBuildVersionHeader.cmake
# Called at build time to generate the private build identity.
#
# Expected -D variables:
#   BML_VERSION         - Stable release version
#   TEMPLATE_FILE       - Path to BuildVersion.h.in
#   OUTPUT_FILE         - Path to output BuildVersion.h
#   SOURCE_DIR          - Git repository root

set(BML_GIT_HASH "unknown")
set(BML_GIT_DIRTY 0)

find_package(Git QUIET)

if(Git_FOUND AND EXISTS "${SOURCE_DIR}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
        WORKING_DIRECTORY "${SOURCE_DIR}"
        OUTPUT_VARIABLE BML_GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE git_hash_result
        ERROR_QUIET
    )
    if(NOT git_hash_result EQUAL 0)
        set(BML_GIT_HASH "unknown")
    endif()

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" diff --quiet HEAD
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE git_dirty_result
    )
    if(NOT git_dirty_result EQUAL 0)
        set(BML_GIT_DIRTY 1)
    endif()
endif()

set(BML_VERSION_FULL "${BML_VERSION}")
if(NOT BML_GIT_HASH STREQUAL "unknown")
    string(APPEND BML_VERSION_FULL "+${BML_GIT_HASH}")
    if(BML_GIT_DIRTY)
        string(APPEND BML_VERSION_FULL ".dirty")
    endif()
endif()

# Generate to a temporary file, then replace the output only when its contents
# changed so the resource compiler does not run on every incremental build.
get_filename_component(output_directory "${OUTPUT_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${output_directory}")
configure_file("${TEMPLATE_FILE}" "${OUTPUT_FILE}.tmp" @ONLY)
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${OUTPUT_FILE}.tmp"
        "${OUTPUT_FILE}"
)
file(REMOVE "${OUTPUT_FILE}.tmp")
