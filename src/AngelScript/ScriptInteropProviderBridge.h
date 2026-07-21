#ifndef BML_SCRIPT_INTEROP_PROVIDER_BRIDGE_H
#define BML_SCRIPT_INTEROP_PROVIDER_BRIDGE_H

#include <cstddef>
#include <memory>

class asIScriptEngine;
class ModContext;

namespace BML {

class ScriptMod;

/* Owns the script callback references retained by registered Interop
 * providers.  It is deliberately a script-runtime service rather than a
 * public C++ API: the cross-DLL provider ABI remains BML/InteropApi.h. */
class ScriptInteropProviderService {
public:
    ScriptInteropProviderService();
    ~ScriptInteropProviderService();

    ScriptInteropProviderService(const ScriptInteropProviderService &) = delete;
    ScriptInteropProviderService &operator=(const ScriptInteropProviderService &) = delete;

    bool Bind(::ModContext *context, ScriptMod *owner);
    /* Opaque bridge objects are intentionally kept out of this service's
     * C++ interface. They exist only as AngelScript reference objects. */
    void *CreateProvider();
    int Register(void *api, void *provider);
    void Release();
    size_t GetActiveCount() const;

private:
    struct State;
    std::shared_ptr<State> m_State;
};

/* Registers only AngelScript-facing provider authoring types.  No symbol from
 * this bridge crosses the BML/native mod DLL boundary. */
int RegisterScriptInteropProviderBridge(asIScriptEngine *engine, const char **errorMessage);

} // namespace BML

#endif // BML_SCRIPT_INTEROP_PROVIDER_BRIDGE_H
