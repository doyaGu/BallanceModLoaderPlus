if(NOT DEFINED DUMPBIN OR NOT EXISTS "${DUMPBIN}")
    message(FATAL_ERROR "Legacy native DLL export ABI test needs a valid dumpbin.exe path.")
endif()
if(NOT DEFINED BINARY OR NOT EXISTS "${BINARY}")
    message(FATAL_ERROR "Legacy native DLL export ABI test needs the built BML DLL.")
endif()
if(NOT DEFINED EXPECTED_COUNT OR NOT DEFINED EXPECTED_SHA256)
    message(FATAL_ERROR "Legacy native DLL export ABI baseline is incomplete.")
endif()

execute_process(
    COMMAND "${DUMPBIN}" /exports "${BINARY}"
    RESULT_VARIABLE dumpbin_status
    OUTPUT_VARIABLE exports
    ERROR_VARIABLE dumpbin_error
)
if(NOT dumpbin_status EQUAL 0)
    message(FATAL_ERROR "dumpbin /exports failed: ${dumpbin_error}")
endif()

# Keep loader-owned legacy C++ interfaces frozen. IMC, BML_* C functions, and
# bundled third-party ImGui exports intentionally belong to separate surfaces.
set(legacy_owner_pattern
        "@(Bui|BGui|ScriptHelper|ExecuteBB|IBML|IMod|IMessageReceiver|ICommand|IConfig|IProperty|ILogger|InputHook)@@")
string(REGEX MATCHALL "[^\r\n]+" export_lines "${exports}")
set(legacy_symbols)
foreach(export_line IN LISTS export_lines)
    if(export_line MATCHES "^[ \t]*[0-9]+[ \t]+[0-9A-F]+[ \t]+[0-9A-F]+[ \t]+([?][^ \t=]+)")
        set(decorated_symbol "${CMAKE_MATCH_1}")
        if(decorated_symbol MATCHES "${legacy_owner_pattern}")
            list(APPEND legacy_symbols "${decorated_symbol}")
        endif()
    endif()
endforeach()

list(REMOVE_DUPLICATES legacy_symbols)
list(SORT legacy_symbols)
list(LENGTH legacy_symbols actual_count)
list(JOIN legacy_symbols "\n" symbol_payload)
string(APPEND symbol_payload "\n")
string(SHA256 actual_sha256 "${symbol_payload}")
string(TOLOWER "${EXPECTED_SHA256}" expected_sha256)

if(NOT actual_count EQUAL EXPECTED_COUNT OR NOT actual_sha256 STREQUAL expected_sha256)
    message(FATAL_ERROR
            "Frozen legacy native DLL exports changed.\n"
            "Expected: ${EXPECTED_COUNT} symbols, SHA-256 ${expected_sha256}\n"
            "Actual:   ${actual_count} symbols, SHA-256 ${actual_sha256}\n"
            "The current major version must preserve decorated MSVC x86 exports.")
endif()

message(STATUS "Verified ${actual_count} frozen legacy native DLL exports.")
