# Interop's public DLL surface is intentionally C-only.  This test is kept
# separate from the C compile probe: a header can look C-compatible while an
# accidental exported C++ helper still leaks into the DLL.

if(NOT DEFINED DUMPBIN OR NOT EXISTS "${DUMPBIN}")
    message(FATAL_ERROR "Interop DLL export ABI test needs a valid dumpbin.exe path.")
endif()
if(NOT DEFINED BINARY OR NOT EXISTS "${BINARY}")
    message(FATAL_ERROR "Interop DLL export ABI test needs the built BML DLL.")
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
        BML_Interop_RegisterProvider
        BML_Interop_RequireApi
        BML_Interop_ReadResource
        BML_Interop_OpenStream
        BML_Interop_RecordBuilder_SetValue
        BML_Imc_OpenClient
        BML_Imc_IsRpcAvailable
        BML_Imc_CallRpc
        BML_Imc_Subscribe
        BML_Imc_Publish)
    string(FIND "${exports}" "${required_export}" required_export_index)
    if(required_export_index EQUAL -1)
        message(FATAL_ERROR "Missing required C Interop export: ${required_export}")
    endif()
endforeach()

# Any export whose name mentions Interop or IMC must use an unmangled public C
# spelling. The rest of BML still has historical C++ exports, so keep this
# check scoped to the two cross-mod ABI families.
string(REGEX MATCHALL "[^\r\n]*(Interop|_Imc_)[^\r\n]*" interop_export_lines "${exports}")
foreach(export_line IN LISTS interop_export_lines)
    if(NOT export_line MATCHES "[ \t](BML_Interop_|BML_Imc_)[A-Za-z0-9_]+([ \t=]|$)")
        message(FATAL_ERROR "Interop/IMC leaked a non-C or mangled DLL export: ${export_line}")
    endif()
endforeach()

foreach(removed_export
        BML_CallModExport
        BML_FindModExport
        BML_RegisterNativeModExport
        BML_RegisterScriptModExport
        BML_InteropCallFrame
        BML_InteropExportRef
        BML_ExportResolver
        BML_EventSubscription
        BML_EventRef
        BML_ObjectId)
    string(FIND "${exports}" "${removed_export}" removed_export_index)
    if(NOT removed_export_index EQUAL -1)
        message(FATAL_ERROR "Removed experimental ABI export is still present: ${removed_export}")
    endif()
endforeach()
