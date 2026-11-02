#pragma once

#if BML_RUNTIME_SDL
#include <CKAll.h>
#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

namespace BML {
// Borrow the engine's registered plugin. Opening another copy by filename can
// create a second AngelScript engine and a different active-context TLS slot.
inline void *FindScriptRuntimeModule() {
#if BML_RUNTIME_SDL
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
#else
    return ::GetModuleHandleA("AngelScript.dll");
#endif
}

inline void *ScriptRuntimeSymbol(void *module, const char *name) {
    if (!module) return nullptr;
#if BML_RUNTIME_SDL
    return static_cast<CKPluginDll *>(module)->GetFunctionPtr(name);
#else
    return reinterpret_cast<void *>(::GetProcAddress(static_cast<HMODULE>(module), name));
#endif
}
}
