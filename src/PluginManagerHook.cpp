#include "PluginManagerHook.h"

#include <MinHook.h>

const char *g_PluginsBlocked[] = {
    "BML",
    "BMLPlus",
    "Hooks",
};

CP_DEFINE_METHOD_HOOK_PTRS(CKPluginManager, ParsePlugins)
CP_DEFINE_METHOD_HOOK_PTRS(CKPluginManager, RegisterPlugin)

#define CP_PLUGIN_MANAGER_METHOD_NAME(Name) CP_HOOK_CLASS_NAME(CKPluginManager)::CP_FUNC_HOOK_NAME(Name)

int CP_PLUGIN_MANAGER_METHOD_NAME(ParsePlugins)(CKSTRING Directory) {
    return CP_CALL_METHOD_ORIG(ParsePlugins, Directory);

}

CKERROR CP_PLUGIN_MANAGER_METHOD_NAME(RegisterPlugin)(CKSTRING str) {
    char filename[MAX_PATH];
    _splitpath(str, nullptr, nullptr, filename, nullptr);
    for (const auto &name : g_PluginsBlocked) {
        if (stricmp(name, filename) == 0)
            return CK_OK;
    }

    return CP_CALL_METHOD_ORIG(RegisterPlugin, str);
}

bool CP_HOOK_CLASS_NAME(CKPluginManager)::InitHooks() {
#define CP_ADD_METHOD_HOOK(Name, Module, Symbol) \
        { FARPROC proc = ::GetProcAddress(::GetModuleHandleA(Module), Symbol); \
          CP_FUNC_TARGET_PTR_NAME(Name) = *(CP_FUNC_TYPE_NAME(Name) *) &proc; } \
        if ((MH_CreateHook(*reinterpret_cast<LPVOID *>(&CP_FUNC_TARGET_PTR_NAME(Name)), \
                           *reinterpret_cast<LPVOID *>(&CP_FUNC_PTR_NAME(Name)), \
                            reinterpret_cast<LPVOID *>(&CP_FUNC_ORIG_PTR_NAME(Name))) != MH_OK || \
            MH_EnableHook(*reinterpret_cast<LPVOID *>(&CP_FUNC_TARGET_PTR_NAME(Name))) != MH_OK)) \
                return false;

    CP_ADD_METHOD_HOOK(ParsePlugins, "CK2.dll", "?ParsePlugins@CKPluginManager@@QAEHPAD@Z")
    CP_ADD_METHOD_HOOK(RegisterPlugin, "CK2.dll", "?RegisterPlugin@CKPluginManager@@QAEJPAD@Z")

    return true;

#undef CP_ADD_METHOD_HOOK
}

void CP_HOOK_CLASS_NAME(CKPluginManager)::ShutdownHooks() {
#define CP_REMOVE_METHOD_HOOK(Name) \
    MH_DisableHook(*reinterpret_cast<void **>(&CP_FUNC_TARGET_PTR_NAME(Name))); \
    MH_RemoveHook(*reinterpret_cast<void **>(&CP_FUNC_TARGET_PTR_NAME(Name)));

    CP_REMOVE_METHOD_HOOK(ParsePlugins)
    CP_REMOVE_METHOD_HOOK(RegisterPlugin)

#undef CP_REMOVE_METHOD_HOOK
}
