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

        /* Unknown error code */
        default:
            return "Unknown error";
    }
}