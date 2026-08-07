if(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
    message(FATAL_ERROR "Legacy native header ABI test needs a valid source root.")
endif()
if(NOT DEFINED MANIFEST OR NOT EXISTS "${MANIFEST}")
    message(FATAL_ERROR "Legacy native header ABI test needs a valid manifest.")
endif()

file(STRINGS "${MANIFEST}" manifest_entries ENCODING UTF-8)
set(checked_header_count 0)

foreach(entry IN LISTS manifest_entries)
    string(STRIP "${entry}" entry)
    if(entry STREQUAL "" OR entry MATCHES "^#")
        continue()
    endif()

    if(NOT entry MATCHES "^([0-9a-fA-F]+)[ \t]+(.+)$")
        message(FATAL_ERROR "Invalid legacy native header manifest entry: ${entry}")
    endif()

    set(expected_hash "${CMAKE_MATCH_1}")
    set(relative_path "${CMAKE_MATCH_2}")
    string(LENGTH "${expected_hash}" expected_hash_length)
    if(NOT expected_hash_length EQUAL 64)
        message(FATAL_ERROR "Invalid SHA-256 in legacy native header manifest: ${entry}")
    endif()
    string(TOLOWER "${expected_hash}" expected_hash)
    set(header_path "${SOURCE_ROOT}/${relative_path}")
    if(NOT EXISTS "${header_path}")
        message(FATAL_ERROR "Frozen legacy native header is missing: ${relative_path}")
    endif()

    file(READ "${header_path}" header_contents)
    string(REPLACE "\r\n" "\n" header_contents "${header_contents}")
    string(REPLACE "\r" "\n" header_contents "${header_contents}")
    string(SHA256 actual_hash "${header_contents}")
    if(NOT actual_hash STREQUAL expected_hash)
        message(FATAL_ERROR
                "Frozen legacy native header changed: ${relative_path}\n"
                "Expected SHA-256: ${expected_hash}\n"
                "Actual SHA-256:   ${actual_hash}\n"
                "The current major version must preserve this native interface.")
    endif()

    math(EXPR checked_header_count "${checked_header_count} + 1")
endforeach()

if(checked_header_count EQUAL 0)
    message(FATAL_ERROR "Legacy native header manifest is empty.")
endif()

message(STATUS "Verified ${checked_header_count} frozen legacy native headers.")
