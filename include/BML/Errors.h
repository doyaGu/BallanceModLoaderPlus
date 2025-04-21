#ifndef BML_ERRORS_H
#define BML_ERRORS_H

#include "BML/Defines.h"
#include "BML/Export.h"

/**
 * @defgroup ErrorCodes Error codes returned by BML functions
 * @{
 */

/* General error codes */
#define BML_OK                              (0)    /**< Operation completed successfully */
#define BML_ERROR_FAIL                      (-1)   /**< General failure */
#define BML_ERROR_FROZEN                    (-2)   /**< Operation cannot be performed in current state */
#define BML_ERROR_NOT_FOUND                 (-3)   /**< Requested item not found */
#define BML_ERROR_NOT_IMPLEMENTED           (-4)   /**< Feature not implemented */
#define BML_ERROR_OUT_OF_MEMORY             (-5)   /**< Memory allocation failed */
#define BML_ERROR_INVALID_PARAMETER         (-6)   /**< Invalid parameter provided */
#define BML_ERROR_ACCESS_DENIED             (-7)   /**< Access to resource denied */
#define BML_ERROR_TIMEOUT                   (-8)   /**< Operation timed out */
#define BML_ERROR_BUSY                      (-9)   /**< Resource busy or locked */
#define BML_ERROR_ALREADY_EXISTS            (-10)  /**< Item already exists */

/* Mod-specific error codes */
#define BML_ERROR_MOD_LOAD_FAILED           (-100) /**< Failed to load mod */
#define BML_ERROR_MOD_INVALID               (-101) /**< Invalid mod format or structure */
#define BML_ERROR_MOD_INCOMPATIBLE          (-102) /**< Mod is incompatible with current BML version */
#define BML_ERROR_MOD_INITIALIZATION        (-103) /**< Mod initialization failed */

/* Dependency-specific error codes */
#define BML_ERROR_DEPENDENCY_CIRCULAR       (-200) /**< Circular dependency detected */
#define BML_ERROR_DEPENDENCY_MISSING        (-201) /**< Required dependency not found */
#define BML_ERROR_DEPENDENCY_VERSION        (-202) /**< Dependency version incompatible */
#define BML_ERROR_DEPENDENCY_RESOLUTION     (-203) /**< Failed to resolve dependencies */
#define BML_ERROR_DEPENDENCY_LIMIT          (-204) /**< Too many dependencies */
#define BML_ERROR_DEPENDENCY_INVALID        (-205) /**< Invalid dependency specification */
#define BML_ERROR_DEPENDENCY_CONFLICT       (-206) /**< Conflicting dependencies detected */

/* Resource-specific error codes */
#define BML_ERROR_RESOURCE_NOT_FOUND        (-300) /**< Resource not found */
#define BML_ERROR_RESOURCE_INVALID          (-301) /**< Invalid resource format */
#define BML_ERROR_RESOURCE_BUSY             (-302) /**< Resource is busy or locked */
#define BML_ERROR_RESOURCE_PERMISSION       (-303) /**< Insufficient permission for resource */

/* Script-specific error codes */
#define BML_ERROR_SCRIPT_INVALID            (-400) /**< Invalid script */
#define BML_ERROR_SCRIPT_EXECUTION          (-401) /**< Script execution failed */
#define BML_ERROR_SCRIPT_TIMEOUT            (-402) /**< Script execution timed out */

/* Command-specific error codes */
#define BML_ERROR_COMMAND_INVALID           (-500) /**< Invalid command */
#define BML_ERROR_COMMAND_PERMISSION        (-501) /**< Insufficient permission for command */
#define BML_ERROR_COMMAND_EXECUTION         (-502) /**< Command execution failed */

/* Configuration-specific error codes */
#define BML_ERROR_CONFIG_INVALID            (-600) /**< Invalid configuration */
#define BML_ERROR_CONFIG_READ               (-601) /**< Failed to read configuration */
#define BML_ERROR_CONFIG_WRITE              (-602) /**< Failed to write configuration */
#define BML_ERROR_CONFIG_FORMAT             (-603) /**< Invalid configuration format */

BML_BEGIN_CDECLS

BML_EXPORT const char* BML_GetErrorString(int errorCode);

BML_END_CDECLS

/** @} */

#endif // BML_ERRORS_H