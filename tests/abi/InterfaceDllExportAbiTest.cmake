if(NOT DEFINED DUMPBIN OR NOT EXISTS "${DUMPBIN}")
    message(FATAL_ERROR "Interface DLL export ABI test needs dumpbin.exe.")
endif()
if(NOT DEFINED BINARY OR NOT EXISTS "${BINARY}")
    message(FATAL_ERROR "Interface DLL export ABI test needs the built BML DLL.")
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

foreach(required_export
        BML_GetInterface
        BML_RegisterInterface
        BML_UnregisterInterface)
    string(FIND "${exports}" "${required_export}" required_export_index)
    if(required_export_index EQUAL -1)
        message(FATAL_ERROR
                "Missing required C interface export: ${required_export}")
    endif()
endforeach()
