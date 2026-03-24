#include "RuntimeProvider.h"

#include <string_view>

#include "bml_services.hpp"

#include "ScriptInstanceManager.h"

namespace BML::Scripting {

static ScriptInstanceManager *g_Manager = nullptr;

static BML_Bool ProviderCanHandle(const char *entry_path) {
    if (!entry_path) return BML_FALSE;
    std::string_view path(entry_path);
    return path.ends_with(".as") ? BML_TRUE : BML_FALSE;
}

static BML_Result ProviderAttach(BML_Mod mod, const BML_Services *services,
                                 const char *entry_path, const char *module_dir) {
    if (!g_Manager) return BML_RESULT_NOT_INITIALIZED;
    return g_Manager->CompileAndAttach(mod, services, entry_path, module_dir);
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

BML_Result RegisterProvider(const BML_Services *services, const char *owner_id) {
    if (!services || !services->HostRuntime || !owner_id)
        return BML_RESULT_INVALID_ARGUMENT;

    auto reg = services->HostRuntime->RegisterRuntimeProvider;
    if (!reg)
        return BML_RESULT_NOT_FOUND;

    return reg(services->HostRuntime->Context, &s_Provider, owner_id);
}

void UnregisterProvider(const BML_Services *services) {
    if (!services || !services->HostRuntime) return;

    auto unreg = services->HostRuntime->UnregisterRuntimeProvider;
    if (unreg) unreg(services->HostRuntime->Context, &s_Provider);
}

} // namespace BML::Scripting
