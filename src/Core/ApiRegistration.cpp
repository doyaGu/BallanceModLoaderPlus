#include "ApiRegistration.h"
#include "ApiRegistry.h"
#include "ApiRegistrationMacros.h"

#include <algorithm>
#include <iterator>

#include "bml_core.h"
#include "bml_api_ids.h"
#include "bml_version.h"
#include "bml_capabilities.h"

#include "Context.h"
#include "ModHandle.h"
#include "ConfigStore.h"
#include "CoreErrors.h"

namespace BML::Core {

namespace {

// ============================================================================
// Helper Functions
// ============================================================================

Context *FromHandle(BML_Context ctx) {
    if (!ctx)
        return nullptr;
    return reinterpret_cast<Context *>(ctx);
}

BML_Mod_T *ResolveMod(BML_Mod mod) {
    auto &ctx = Context::Instance();
    auto target = mod ? mod : Context::GetCurrentModule();
    return ctx.ResolveModHandle(target);
}

// ============================================================================
// Core API Implementations
// ============================================================================

BML_Result BML_API_ContextRetain(BML_Context ctx) {
    auto *context = FromHandle(ctx);
    if (!context)
        return BML_RESULT_INVALID_ARGUMENT;
    return context->RetainHandle();
}

BML_Result BML_API_ContextRelease(BML_Context ctx) {
    auto *context = FromHandle(ctx);
    if (!context)
        return BML_RESULT_INVALID_ARGUMENT;
    return context->ReleaseHandle();
}

BML_Context BML_API_GetGlobalContext() {
    return Context::Instance().GetHandle();
}

const BML_Version *BML_API_GetRuntimeVersion() {
    return &Context::Instance().GetRuntimeVersion();
}

BML_Result BML_API_RequestCapability(BML_Mod mod, const char *capability_id) {
    if (!capability_id)
        return BML_RESULT_INVALID_ARGUMENT;
    auto *handle = ResolveMod(mod);
    if (!handle)
        return BML_RESULT_INVALID_ARGUMENT;
    auto it = std::find(handle->capabilities.begin(), handle->capabilities.end(), capability_id);
    return it != handle->capabilities.end() ? BML_RESULT_OK : BML_RESULT_NOT_FOUND;
}

BML_Result BML_API_CheckCapability(BML_Mod mod, const char *capability_id, BML_Bool *out_supported) {
    if (out_supported)
        *out_supported = BML_FALSE;
    if (!capability_id)
        return BML_RESULT_INVALID_ARGUMENT;
    auto *handle = ResolveMod(mod);
    if (!handle)
        return BML_RESULT_INVALID_ARGUMENT;
    bool supported = std::find(handle->capabilities.begin(), handle->capabilities.end(), capability_id) != handle->capabilities.end();
    if (out_supported)
        *out_supported = supported ? BML_TRUE : BML_FALSE;
    return BML_RESULT_OK;
}

BML_Result BML_API_GetModId(BML_Mod mod, const char **out_id) {
    if (!out_id)
        return BML_RESULT_INVALID_ARGUMENT;
    auto *handle = ResolveMod(mod);
    if (!handle)
        return BML_RESULT_INVALID_ARGUMENT;
    *out_id = handle->id.c_str();
    return BML_RESULT_OK;
}

BML_Result BML_API_GetModVersion(BML_Mod mod, BML_Version *out_version) {
    if (!out_version)
        return BML_RESULT_INVALID_ARGUMENT;
    auto *handle = ResolveMod(mod);
    if (!handle)
        return BML_RESULT_INVALID_ARGUMENT;
    *out_version = handle->version;
    return BML_RESULT_OK;
}

BML_Result BML_API_RegisterShutdownHook(BML_Mod mod, BML_ShutdownCallback callback, void *user_data) {
    if (!callback)
        return BML_RESULT_INVALID_ARGUMENT;
    auto *handle = ResolveMod(mod);
    if (!handle)
        return BML_RESULT_INVALID_ARGUMENT;
    Context::Instance().AppendShutdownHook(handle, callback, user_data);
    return BML_RESULT_OK;
}

BML_Result BML_API_SetCurrentModule(BML_Mod mod) {
    if (!mod) {
        Context::SetCurrentModule(nullptr);
        return BML_RESULT_OK;
    }
    auto *handle = Context::Instance().ResolveModHandle(mod);
    if (!handle)
        return BML_RESULT_INVALID_ARGUMENT;
    Context::SetCurrentModule(handle);
    return BML_RESULT_OK;
}

BML_Mod BML_API_GetCurrentModule() {
    return Context::GetCurrentModule();
}

BML_Result BML_API_GetCoreCaps(BML_CoreCaps *out_caps) {
    if (!out_caps)
        return BML_RESULT_INVALID_ARGUMENT;
    BML_CoreCaps caps{};
    caps.struct_size = sizeof(BML_CoreCaps);
    caps.runtime_version = Context::Instance().GetRuntimeVersion();
    caps.api_version = bmlGetApiVersion();
    caps.capability_flags = BML_CORE_CAP_CONTEXT_RETAIN |
                            BML_CORE_CAP_RUNTIME_QUERY |
                            BML_CORE_CAP_MOD_METADATA |
                            BML_CORE_CAP_SHUTDOWN_HOOKS |
                            BML_CORE_CAP_CAPABILITY_CHECKS |
                            BML_CORE_CAP_CURRENT_MODULE_TLS;
    caps.threading_model = BML_THREADING_FREE;  // Core APIs are fully thread-safe
    *out_caps = caps;
    return BML_RESULT_OK;
}

// ============================================================================
// Core API Subsystem Descriptors
// ============================================================================

enum CoreApiMask : uint32_t {
    CORE_API_LOGGING = 1u << 0,
    CORE_API_CONFIG = 1u << 1,
    CORE_API_IMC = 1u << 2,
    CORE_API_RESOURCE = 1u << 3,
    CORE_API_EXTENSION = 1u << 4,
    CORE_API_MEMORY = 1u << 5,
    CORE_API_DIAGNOSTIC = 1u << 6,
    CORE_API_SYNC = 1u << 7,
    CORE_API_PROFILING = 1u << 8,
    CORE_API_CAPABILITY = 1u << 9,
    CORE_API_TRACING = 1u << 10,
};

constexpr ApiRegistry::CoreApiDescriptor kCoreApiDescriptors[] = {
    {"Logging", RegisterLoggingApis, CORE_API_LOGGING, 0u},
    {"ConfigStore", RegisterConfigApis, CORE_API_CONFIG, CORE_API_LOGGING},
    {"ImcBus", RegisterImcApis, CORE_API_IMC, CORE_API_LOGGING},
    {"Resource", RegisterResourceApis, CORE_API_RESOURCE, CORE_API_LOGGING},
    {"Extension", RegisterExtensionApis, CORE_API_EXTENSION, CORE_API_CONFIG | CORE_API_RESOURCE},
    {"Memory", RegisterMemoryApis, CORE_API_MEMORY, 0u},
    {"Diagnostic", RegisterDiagnosticApis, CORE_API_DIAGNOSTIC, 0u},
    {"Sync", RegisterSyncApis, CORE_API_SYNC, 0u},
    {"Profiling", RegisterProfilingApis, CORE_API_PROFILING, 0u},
    {"Capability", RegisterCapabilityApis, CORE_API_CAPABILITY, 0u},
    {"Tracing", RegisterTracingApis, CORE_API_TRACING, CORE_API_LOGGING},
};

} // namespace

void RegisterCoreApis() {
    BML_BEGIN_API_REGISTRATION();

    // Context management APIs
    BML_REGISTER_API_GUARDED_WITH_CAPS(bmlContextRetain, "core.context", BML_API_ContextRetain, BML_CAP_CONTEXT);
    BML_REGISTER_API_GUARDED_WITH_CAPS(bmlContextRelease, "core.context", BML_API_ContextRelease, BML_CAP_CONTEXT);
    BML_REGISTER_API_WITH_CAPS(bmlGetGlobalContext, BML_API_GetGlobalContext, BML_CAP_CONTEXT);
    BML_REGISTER_API_WITH_CAPS(bmlGetRuntimeVersion, BML_API_GetRuntimeVersion, BML_CAP_RUNTIME);

    // Capability APIs
    BML_REGISTER_API_GUARDED_WITH_CAPS(bmlRequestCapability, "core.capabilities", BML_API_RequestCapability, BML_CAP_CAPABILITY_QUERY);
    BML_REGISTER_API_GUARDED_WITH_CAPS(bmlCheckCapability, "core.capabilities", BML_API_CheckCapability, BML_CAP_CAPABILITY_QUERY);

    // Module metadata APIs
    BML_REGISTER_API_GUARDED_WITH_CAPS(bmlGetModId, "core.metadata", BML_API_GetModId, BML_CAP_MOD_INFO);
    BML_REGISTER_API_GUARDED_WITH_CAPS(bmlGetModVersion, "core.metadata", BML_API_GetModVersion, BML_CAP_MOD_INFO);

    // Lifecycle APIs
    BML_REGISTER_API_GUARDED_WITH_CAPS(bmlRegisterShutdownHook, "core.lifecycle", BML_API_RegisterShutdownHook, BML_CAP_LIFECYCLE);
    BML_REGISTER_API_GUARDED_WITH_CAPS(bmlSetCurrentModule, "core.lifecycle", BML_API_SetCurrentModule, BML_CAP_MOD_INFO);
    BML_REGISTER_API_WITH_CAPS(bmlGetCurrentModule, BML_API_GetCurrentModule, BML_CAP_MOD_INFO);

    // Runtime capabilities API
    BML_REGISTER_CAPS_API_WITH_CAPS(bmlGetCoreCaps, "core.runtime", BML_API_GetCoreCaps, BML_CAP_RUNTIME);

    // Error handling APIs
    BML_REGISTER_API_WITH_CAPS(bmlGetLastError, GetLastErrorInfo, BML_CAP_DIAGNOSTICS);
    BML_REGISTER_API_WITH_CAPS(bmlClearLastError, ClearLastErrorInfo, BML_CAP_DIAGNOSTICS);
    BML_REGISTER_API_WITH_CAPS(bmlGetErrorString, GetErrorString, BML_CAP_DIAGNOSTICS);

    // Register all subsystem APIs in dependency order
    registry.RegisterCoreApiSet(kCoreApiDescriptors, std::size(kCoreApiDescriptors));
}

} // namespace BML::Core
