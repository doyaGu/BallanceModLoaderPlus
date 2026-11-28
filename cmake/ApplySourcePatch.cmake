# FetchContent may rerun the patch step on an existing checkout.
execute_process(COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${PATCH_FILE}"
    RESULT_VARIABLE already_applied OUTPUT_QUIET ERROR_QUIET)
if(NOT already_applied EQUAL 0)
    execute_process(COMMAND "${GIT_EXECUTABLE}" apply "${PATCH_FILE}"
        COMMAND_ERROR_IS_FATAL ANY)
endif()
