# Finalize only the Mod produced by bml_add_mod. Do not modify or deploy SDL:
# Player supplies it, and loading another copy creates independent SDL state.
if(NOT EXISTS "${BML_MOD_FILE}")
    message(FATAL_ERROR "SDL Mod does not exist: ${BML_MOD_FILE}")
endif()
find_program(_otool otool REQUIRED)
find_program(_install_name_tool install_name_tool REQUIRED)
find_program(_codesign codesign REQUIRED)
execute_process(COMMAND "${_otool}" -L "${BML_MOD_FILE}"
    OUTPUT_VARIABLE _dependencies COMMAND_ERROR_IS_FATAL ANY)
string(REPLACE "\n" ";" _lines "${_dependencies}")
set(_changes)
foreach(_line IN LISTS _lines)
    if(NOT _line MATCHES "^[\t ]+(.+) \\(compatibility version ")
        continue()
    endif()
    set(_dependency "${CMAKE_MATCH_1}")
    get_filename_component(_name "${_dependency}" NAME)
    if(_name MATCHES "^libSDL3(\\.[0-9]+)*\\.dylib$" AND
       NOT _dependency STREQUAL "@rpath/${_name}")
        list(APPEND _changes -change "${_dependency}" "@rpath/${_name}")
    endif()
endforeach()
if(_changes)
    execute_process(COMMAND "${_install_name_tool}" ${_changes} "${BML_MOD_FILE}"
        COMMAND_ERROR_IS_FATAL ANY)
    # Changing load commands invalidates the linker's signature on Apple Silicon.
    # Consumers may apply their distribution identity after this build step.
    execute_process(COMMAND "${_codesign}" --force --sign - "${BML_MOD_FILE}"
        COMMAND_ERROR_IS_FATAL ANY)
endif()
