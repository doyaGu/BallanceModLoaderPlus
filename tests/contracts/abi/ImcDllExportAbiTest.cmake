if(NOT DEFINED DUMPBIN OR NOT EXISTS "${DUMPBIN}")
    message(FATAL_ERROR "IMC DLL export ABI test needs a valid dumpbin.exe path.")
endif()
if(NOT DEFINED BINARY OR NOT EXISTS "${BINARY}")
    message(FATAL_ERROR "IMC DLL export ABI test needs the built BML DLL.")
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
        BML_Imc_OpenClient
        BML_Imc_IsRpcAvailable
        BML_Imc_CallRpc
        BML_Imc_Subscribe
        BML_Imc_Publish)
    string(FIND "${exports}" "${required_export}" required_export_index)
    if(required_export_index EQUAL -1)
        message(FATAL_ERROR "Missing required C IMC export: ${required_export}")
    endif()
endforeach()

string(REGEX MATCHALL "[^\r\n]*(_Imc_|Interop)[^\r\n]*" imc_export_lines "${exports}")
foreach(export_line IN LISTS imc_export_lines)
    if(export_line MATCHES "BML_Interop_")
        message(FATAL_ERROR "Removed Interop export is still present: ${export_line}")
    endif()
    if(export_line MATCHES "_Imc_" AND
       NOT export_line MATCHES "[ \t]BML_Imc_[A-Za-z0-9_]+([ \t=]|$)")
        message(FATAL_ERROR "IMC leaked a non-C or mangled DLL export: ${export_line}")
    endif()
endforeach()
