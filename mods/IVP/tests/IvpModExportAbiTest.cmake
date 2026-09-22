if(NOT DEFINED DUMPBIN OR NOT EXISTS "${DUMPBIN}")
    message(FATAL_ERROR "IVP Mod export ABI test needs dumpbin.exe.")
endif()
if(NOT DEFINED BINARY OR NOT EXISTS "${BINARY}")
    message(FATAL_ERROR "IVP Mod export ABI test needs the built IVP.bmodp.")
endif()

execute_process(
        COMMAND "${DUMPBIN}" /exports "${BINARY}"
        RESULT_VARIABLE dumpbin_status
        OUTPUT_VARIABLE exports
        ERROR_VARIABLE dumpbin_error)
if(NOT dumpbin_status EQUAL 0)
    message(FATAL_ERROR "dumpbin /exports failed: ${dumpbin_error}")
endif()

string(REGEX MATCHALL
        "[\r\n][ \t]+[0-9]+[ \t]+[0-9A-F]+[ \t]+[0-9A-F]+[ \t]+([^ \t\r\n=]+)"
        export_rows "${exports}")
set(export_names)
foreach(export_row IN LISTS export_rows)
    string(REGEX REPLACE
            ".*[ \t]([^ \t\r\n=]+)$" "\\1" export_name "${export_row}")
    list(APPEND export_names "${export_name}")
endforeach()
list(SORT export_names)

set(expected_exports BMLEntry BMLExit)
list(SORT expected_exports)
if(NOT export_names STREQUAL expected_exports)
    list(JOIN export_names ", " actual_report)
    message(FATAL_ERROR
            "IVP.bmodp must export only BMLEntry and BMLExit; found: "
            "${actual_report}")
endif()

foreach(forbidden IN ITEMS
        IVP_GetInterface
        BML_IVP_GetInterface
        IVP_QueryInterface)
    string(FIND "${exports}" "${forbidden}" forbidden_index)
    if(NOT forbidden_index EQUAL -1)
        message(FATAL_ERROR
                "IVP.bmodp exposes forbidden API symbol: ${forbidden}")
    endif()
endforeach()

message(STATUS "Verified IVP.bmodp exports only BMLEntry and BMLExit.")
