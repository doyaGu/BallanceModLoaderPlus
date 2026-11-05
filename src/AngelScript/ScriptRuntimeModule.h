#pragma once

#include <CKAll.h>

namespace BML {
// Borrow the engine's registered plugin. Opening another copy by filename can
// create a second AngelScript engine and a different active-context TLS slot.
inline void *FindScriptRuntimeModule() {
    auto *plugins = CKGetPluginManager();
    if (!plugins) return nullptr;
    CKPluginDll *runtime = nullptr;
    for (int i = 0; i < plugins->GetPluginDllCount(); ++i) {
        auto *plugin = plugins->GetPluginDllInfo(i);
        if (!plugin || !plugin->GetFunctionPtr("CKGetAngelScript")) continue;
        if (runtime) return nullptr; // Ambiguous runtimes must not be mixed.
        runtime = plugin;
    }
    return runtime;
}

inline void *ScriptRuntimeSymbol(void *module, const char *name) {
    if (!module) return nullptr;
    return static_cast<CKPluginDll *>(module)->GetFunctionPtr(name);
}
}
