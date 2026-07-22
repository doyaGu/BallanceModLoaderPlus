#ifndef BML_SCRIPT_IMC_FACADE_H
#define BML_SCRIPT_IMC_FACADE_H

class asIScriptEngine;

/* Registers the generated API consumer facade used by ordinary script
 * mods.  The raw C ABI, records, and handles remain implementation details;
 * scripts receive small value snapshots and guarded borrowed CKObject@ values.
 */
int RegisterScriptImcFacade(asIScriptEngine *engine, const char **errorMessage);

#endif // BML_SCRIPT_IMC_FACADE_H
