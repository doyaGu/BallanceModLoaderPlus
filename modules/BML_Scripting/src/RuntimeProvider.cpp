#include "RuntimeProvider.h"

#include <string_view>

#include "ScriptInstanceManager.h"

namespace BML::Scripting {

static ScriptInstanceManager *g_Manager = nullptr;

static BML_Bool ProviderCanHandle(const char *entry_path) {
    if (!entry_path) return BML_FALSE;
    std::string_view path(entry_path);
    return path.ends_with(".as") ? BML_TRUE : BML_FALSE;
}

static BML_Result ProviderAttach(BML_Mod mod, PFN_BML_GetProcAddress get_proc,
                                  const char *entry_path, const char *module_dir) {
    if (!g_Manager) return BML_RESULT_NOT_INITIALIZED;
    return g_Manager->CompileAndAttach(mod, get_proc, entry_path, module_dir);
}

static BML_Result ProviderPrepareDetach(BML_Mod mod) {
    if (!g_Manager) return BML_RESULT_OK;
    return g_Manager->PrepareDetach(mod);
}

static BML_Result ProviderDetach(BML_Mod mod) {
    if (!g_Manager) return BML_RESULT_OK;
    return g_Manager->Detach(mod);
}

static BML_Result ProviderReload(BML_Mod mod) {
    if (!g_Manager) return BML_RESULT_NOT_INITIALIZED;
    return g_Manager->Reload(mod);
}

static const BML_ModuleRuntimeProvider s_Provider = {
    BML_MODULE_RUNTIME_PROVIDER_INIT,
    ProviderCanHandle,
    ProviderAttach,
    ProviderPrepareDetach,
    ProviderDetach,
    ProviderReload,
};

const BML_ModuleRuntimeProvider *GetRuntimeProvider() {
    return &s_Provider;
}

void SetInstanceManager(ScriptInstanceManager *manager) {
    g_Manager = manager;
}

BML_Result RegisterProvider(PFN_BML_GetProcAddress get_proc, const char *owner_id) {
    if (!get_proc || !owner_id)
        return BML_RESULT_INVALID_ARGUMENT;

    auto reg = reinterpret_cast<PFN_BML_RegisterRuntimeProvider>(
        get_proc("bmlRegisterRuntimeProvider"));
    if (!reg)
        return BML_RESULT_NOT_FOUND;

    return reg(&s_Provider, owner_id);
}

void UnregisterProvider(PFN_BML_GetProcAddress get_proc) {
    if (!get_proc) return;

    auto unreg = reinterpret_cast<PFN_BML_UnregisterRuntimeProvider>(
        get_proc("bmlUnregisterRuntimeProvider"));
    if (unreg) unreg(&s_Provider);
}

} // namespace BML::Scripting
