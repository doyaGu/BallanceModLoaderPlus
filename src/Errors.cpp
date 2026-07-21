#include "BML/Defines.h"


const char *BML_GetErrorString(int errorCode) {
    switch (errorCode) {
        /* General error codes */
        case BML_OK:
            return "Success";
        case BML_ERROR_FAIL:
            return "Operation failed";
        case BML_ERROR_FROZEN:
            return "Operation cannot be performed in current state";
        case BML_ERROR_NOT_FOUND:
            return "Requested item not found";
        case BML_ERROR_NOT_IMPLEMENTED:
            return "Feature not implemented";
        case BML_ERROR_OUT_OF_MEMORY:
            return "Memory allocation failed";
        case BML_ERROR_INVALID_PARAMETER:
            return "Invalid parameter provided";
        case BML_ERROR_ACCESS_DENIED:
            return "Access to resource denied";
        case BML_ERROR_TIMEOUT:
            return "Operation timed out";
        case BML_ERROR_BUSY:
            return "Resource busy or locked";
        case BML_ERROR_ALREADY_EXISTS:
            return "Item already exists";
        case BML_ERROR_INVALID_HANDLE:
            return "Invalid or stale handle";
        case BML_ERROR_WOULD_BLOCK:
            return "Operation would block";
        case BML_ERROR_CANCELLED:
            return "Operation cancelled";
        case BML_ERROR_WRONG_THREAD:
            return "Operation is not allowed on this thread";
        case BML_ERROR_MALFORMED_MESSAGE:
            return "Malformed encoded message";
        case BML_ERROR_TYPE_MISMATCH:
            return "Encoded field type mismatch";

        /* Mod-specific error codes */
        case BML_ERROR_MOD_LOAD_FAILED:
            return "Failed to load mod";
        case BML_ERROR_MOD_INVALID:
            return "Invalid mod format or structure";
        case BML_ERROR_MOD_INCOMPATIBLE:
            return "Mod is incompatible with current BML version";
        case BML_ERROR_MOD_INITIALIZATION:
            return "Mod initialization failed";

        /* Dependency-specific error codes */
        case BML_ERROR_DEPENDENCY_CIRCULAR:
            return "Circular dependency detected";
        case BML_ERROR_DEPENDENCY_MISSING:
            return "Required dependency not found";
        case BML_ERROR_DEPENDENCY_VERSION:
            return "Dependency version incompatible";
        case BML_ERROR_DEPENDENCY_RESOLUTION:
            return "Failed to resolve dependencies";
        case BML_ERROR_DEPENDENCY_LIMIT:
            return "Too many dependencies";
        case BML_ERROR_DEPENDENCY_INVALID:
            return "Invalid dependency specification";
        case BML_ERROR_DEPENDENCY_CONFLICT:
            return "Conflicting dependencies detected";

        /* Resource-specific error codes */
        case BML_ERROR_RESOURCE_NOT_FOUND:
            return "Resource not found";
        case BML_ERROR_RESOURCE_INVALID:
            return "Invalid resource format";
        case BML_ERROR_RESOURCE_BUSY:
            return "Resource is busy or locked";
        case BML_ERROR_RESOURCE_PERMISSION:
            return "Insufficient permission for resource";

        /* Script-specific error codes */
        case BML_ERROR_SCRIPT_INVALID:
            return "Invalid script";
        case BML_ERROR_SCRIPT_EXECUTION:
            return "Script execution failed";
        case BML_ERROR_SCRIPT_TIMEOUT:
            return "Script execution timed out";

        /* Command-specific error codes */
        case BML_ERROR_COMMAND_INVALID:
            return "Invalid command";
        case BML_ERROR_COMMAND_PERMISSION:
            return "Insufficient permission for command";
        case BML_ERROR_COMMAND_EXECUTION:
            return "Command execution failed";

        /* Configuration-specific error codes */
        case BML_ERROR_CONFIG_INVALID:
            return "Invalid configuration";
        case BML_ERROR_CONFIG_READ:
            return "Failed to read configuration";
        case BML_ERROR_CONFIG_WRITE:
            return "Failed to write configuration";
        case BML_ERROR_CONFIG_FORMAT:
            return "Invalid configuration format";

        /* Interop-specific error codes */
        case BML_ERROR_INTEROP_ENDPOINT_NOT_FOUND:
            return "Interop API endpoint not found";
        case BML_ERROR_INTEROP_VALUE_TYPE_MISMATCH:
            return "Interop record value type mismatch";
        case BML_ERROR_INTEROP_HANDLE_STALE:
            return "Interop handle is stale";
        case BML_ERROR_INTEROP_UNSUPPORTED:
            return "Interop operation is unsupported by the current runtime";
        case BML_ERROR_INTEROP_API_INVALID:
            return "Interop API descriptor is invalid";
        case BML_ERROR_INTEROP_API_MISMATCH:
            return "Interop API is incompatible with the requested API";
        case BML_ERROR_INTEROP_PROVIDER_UNLOADED:
            return "Interop provider was unloaded";
        case BML_ERROR_INTEROP_WRONG_THREAD:
            return "Interop provider requires the game thread";
        case BML_ERROR_INTEROP_RECORD_INVALID:
            return "Interop record is invalid";
        case BML_ERROR_INTEROP_CURSOR_STALE:
            return "Interop collection cursor is stale";
        case BML_ERROR_INTEROP_OBJECT_INVALID:
            return "Interop object reference is invalid or expired";
        case BML_ERROR_INTEROP_SCHEMA_MISMATCH:
            return "Interop record schema does not match the API endpoint";
        case BML_ERROR_INTEROP_TARGET_EXECUTION_FAILED:
            return "Interop provider callback execution failed";

        /* Unknown error code */
        default:
            return "Unknown error";
    }
}
