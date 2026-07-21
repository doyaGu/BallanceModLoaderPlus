#ifndef BML_SCRIPT_INTEROP_CONSUMER_BRIDGE_H
#define BML_SCRIPT_INTEROP_CONSUMER_BRIDGE_H

class asIScriptEngine;

namespace BML {

/* Registers the advanced, API-agnostic consumer bridge in BML::Interop.
 * Generated .bmlapi bindings build their typed script facades on this private
 * host layer; no C++ type is exported across the native mod ABI. */
int RegisterScriptInteropConsumerBridge(asIScriptEngine *engine, const char **errorMessage);

} // namespace BML

#endif // BML_SCRIPT_INTEROP_CONSUMER_BRIDGE_H
