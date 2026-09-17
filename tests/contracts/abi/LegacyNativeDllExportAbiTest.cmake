if(NOT DEFINED DUMPBIN OR NOT EXISTS "${DUMPBIN}")
    message(FATAL_ERROR "Legacy native DLL export ABI test needs a valid dumpbin.exe path.")
endif()
if(NOT DEFINED BINARY OR NOT EXISTS "${BINARY}")
    message(FATAL_ERROR "Legacy native DLL export ABI test needs the built BML DLL.")
endif()
if(NOT DEFINED BASELINE OR NOT EXISTS "${BASELINE}")
    message(FATAL_ERROR "Legacy native DLL export ABI test needs a symbol baseline.")
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

file(STRINGS "${BASELINE}" baseline_entries ENCODING UTF-8)
set(expected_symbols)
foreach(entry IN LISTS baseline_entries)
    string(STRIP "${entry}" entry)
    if(NOT entry STREQUAL "" AND NOT entry MATCHES "^#")
        list(APPEND expected_symbols "${entry}")
    endif()
endforeach()
list(LENGTH expected_symbols baseline_entry_count)
list(REMOVE_DUPLICATES expected_symbols)
list(SORT expected_symbols)
list(LENGTH expected_symbols expected_count)
if(NOT baseline_entry_count EQUAL expected_count OR expected_count EQUAL 0)
    message(FATAL_ERROR
            "Legacy native DLL export ABI baseline is empty or contains duplicates.")
endif()

set(missing_symbols)
foreach(expected_symbol IN LISTS expected_symbols)
    if(NOT expected_symbol IN_LIST legacy_symbols)
        list(APPEND missing_symbols "${expected_symbol}")
    endif()
endforeach()

set(added_symbols)
foreach(actual_symbol IN LISTS legacy_symbols)
    if(NOT actual_symbol IN_LIST expected_symbols)
        list(APPEND added_symbols "${actual_symbol}")
    endif()
endforeach()

if(missing_symbols OR added_symbols)
    list(LENGTH legacy_symbols actual_count)
    list(JOIN missing_symbols "\n  - " missing_report)
    list(JOIN added_symbols "\n  + " added_report)
    if(NOT missing_report STREQUAL "")
        string(PREPEND missing_report "  - ")
    else()
        set(missing_report "  (none)")
    endif()
    if(NOT added_report STREQUAL "")
        string(PREPEND added_report "  + ")
    else()
        set(added_report "  (none)")
    endif()
    message(FATAL_ERROR
            "Frozen legacy native DLL exports changed.\n"
            "Expected ${expected_count} symbols; found ${actual_count}.\n"
            "Missing:\n${missing_report}\n"
            "Added:\n${added_report}\n"
            "The current major version must preserve decorated MSVC x86 exports.")
endif()

message(STATUS "Verified ${expected_count} frozen legacy native DLL exports.")
