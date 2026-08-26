#ifndef BML_SCRIPT_BUILTIN_FACADE_H
#define BML_SCRIPT_BUILTIN_FACADE_H

class asIScriptEngine;

/* Registers the script side of the loader's built-in capabilities.  The C ABI
 * and interface structs remain implementation details; scripts receive small
 * value snapshots and guarded borrowed CKObject@ values.
 */
int RegisterScriptBuiltinFacade(asIScriptEngine *engine, const char **errorMessage);

#endif // BML_SCRIPT_BUILTIN_FACADE_H
