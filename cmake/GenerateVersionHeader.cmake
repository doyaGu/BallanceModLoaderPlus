# GenerateVersionHeader.cmake
# Called at build time to generate Version.h with git metadata.
#
# Expected -D variables:
#   BML_VERSION_MAJOR   - Major version number
#   BML_VERSION_MINOR   - Minor version number
#   BML_VERSION_PATCH   - Patch version number
#   TEMPLATE_FILE       - Path to Version.h.in
#   OUTPUT_FILE         - Path to output Version.h
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

# Build version strings
set(BML_VERSION "${BML_VERSION_MAJOR}.${BML_VERSION_MINOR}.${BML_VERSION_PATCH}")

set(BML_VERSION_FULL "${BML_VERSION}")
if(NOT BML_GIT_HASH STREQUAL "unknown")
    string(APPEND BML_VERSION_FULL "+${BML_GIT_HASH}")
    if(BML_GIT_DIRTY)
        string(APPEND BML_VERSION_FULL ".dirty")
    endif()
endif()

# Generate to temp file, then copy only if changed to avoid unnecessary rebuilds
configure_file("${TEMPLATE_FILE}" "${OUTPUT_FILE}.tmp" @ONLY)
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${OUTPUT_FILE}.tmp"
        "${OUTPUT_FILE}"
)
file(REMOVE "${OUTPUT_FILE}.tmp")
