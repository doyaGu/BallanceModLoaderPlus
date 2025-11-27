#include "ApiRegistration.h"
#include "ApiRegistry.h"

#include <algorithm>
#include <iterator>

#include "bml_core.h"
#include "bml_api_ids.h"
#include "bml_version.h"

#include "Context.h"
#include "ModHandle.h"
#include "ConfigStore.h"
#include "CoreErrors.h"

namespace BML::Core {

namespace {

Context *FromHandle(BML_Context ctx) {
    if (!ctx)
        return nullptr;
    return reinterpret_cast<Context *>(ctx);
}

BML_Result ContextRetain(BML_Context ctx) {
    auto *context = FromHandle(ctx);
    if (!context)
        return BML_RESULT_INVALID_ARGUMENT;
    return context->RetainHandle();
}

BML_Result ContextRelease(BML_Context ctx) {
    auto *context = FromHandle(ctx);
    if (!context)
        return BML_RESULT_INVALID_ARGUMENT;
    return context->ReleaseHandle();
}

BML_Context GetGlobalContext() {
    return Context::Instance().GetHandle();
}

const BML_Version *GetRuntimeVersion() {
    return &Context::Instance().GetRuntimeVersion();
}

BML_Mod_T *ResolveMod(BML_Mod mod) {
    auto &ctx = Context::Instance();
    auto target = mod ? mod : Context::GetCurrentModule();
    return ctx.ResolveModHandle(target);
}

BML_Result RequestCapability(BML_Mod mod, const char *capability_id) {
    if (!capability_id)
        return BML_RESULT_INVALID_ARGUMENT;
    auto *handle = ResolveMod(mod);
    if (!handle)
        return BML_RESULT_INVALID_ARGUMENT;
    auto it = std::find(handle->capabilities.begin(), handle->capabilities.end(), capability_id);
    return it != handle->capabilities.end() ? BML_RESULT_OK : BML_RESULT_NOT_FOUND;
}

BML_Result CheckCapability(BML_Mod mod, const char *capability_id, BML_Bool *out_supported) {
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

BML_Result GetModId(BML_Mod mod, const char **out_id) {
    if (!out_id)
        return BML_RESULT_INVALID_ARGUMENT;
    auto *handle = ResolveMod(mod);
    if (!handle)
        return BML_RESULT_INVALID_ARGUMENT;
    *out_id = handle->id.c_str();
    return BML_RESULT_OK;
}

BML_Result GetModVersion(BML_Mod mod, BML_Version *out_version) {
    if (!out_version)
        return BML_RESULT_INVALID_ARGUMENT;
    auto *handle = ResolveMod(mod);
    if (!handle)
        return BML_RESULT_INVALID_ARGUMENT;
    *out_version = handle->version;
    return BML_RESULT_OK;
}

BML_Result RegisterShutdownHook(BML_Mod mod, BML_ShutdownCallback callback, void *user_data) {
    if (!callback)
        return BML_RESULT_INVALID_ARGUMENT;
    auto *handle = ResolveMod(mod);
    if (!handle)
        return BML_RESULT_INVALID_ARGUMENT;
    Context::Instance().AppendShutdownHook(handle, callback, user_data);
    return BML_RESULT_OK;
}

BML_Result SetCurrentModule(BML_Mod mod) {
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

BML_Mod GetCurrentModuleHandle() {
    return Context::GetCurrentModule();
}

BML_Result GetCoreCaps(BML_CoreCaps *out_caps) {
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
    auto &registry = ApiRegistry::Instance();

    auto register_core_api = [&](const char* name, BML_ApiId id, void* pointer) {
        ApiRegistry::ApiMetadata meta{};
        meta.name = name;
        meta.id = id;
        meta.pointer = pointer;
        meta.version_major = BML_API_VERSION_MAJOR;
        meta.version_minor = BML_API_VERSION_MINOR;
        meta.version_patch = BML_API_VERSION_PATCH;
        meta.type = BML_API_TYPE_CORE;
        meta.threading = BML_THREADING_FREE;
        meta.provider_mod = "BML";
        registry.RegisterApi(meta);
    };

    register_core_api("bmlContextRetain", BML_API_ID_bmlContextRetain, reinterpret_cast<void *>(+[](BML_Context ctx) {
        return GuardResult("core.context", [ctx] {
            return ContextRetain(ctx);
        });
    }));
    register_core_api("bmlContextRelease", BML_API_ID_bmlContextRelease, reinterpret_cast<void *>(+[](BML_Context ctx) {
        return GuardResult("core.context", [ctx] {
            return ContextRelease(ctx);
        });
    }));
    register_core_api("bmlGetGlobalContext", BML_API_ID_bmlGetGlobalContext, reinterpret_cast<void *>(GetGlobalContext));
    register_core_api("bmlGetRuntimeVersion", BML_API_ID_bmlGetRuntimeVersion, reinterpret_cast<void *>(GetRuntimeVersion));
    register_core_api("bmlRequestCapability", BML_API_ID_bmlRequestCapability, reinterpret_cast<void *>(+[](BML_Mod mod, const char *capability_id) {
        return GuardResult("core.capabilities", [mod, capability_id] {
            return RequestCapability(mod, capability_id);
        });
    }));
    register_core_api("bmlCheckCapability", BML_API_ID_bmlCheckCapability, reinterpret_cast<void *>(+[](BML_Mod mod, const char *capability_id, BML_Bool *out_supported) {
        return GuardResult("core.capabilities", [mod, capability_id, out_supported] {
            return CheckCapability(mod, capability_id, out_supported);
        });
    }));
    register_core_api("bmlGetModId", BML_API_ID_bmlGetModId, reinterpret_cast<void *>(+[](BML_Mod mod, const char **out_id) {
        return GuardResult("core.metadata", [mod, out_id] {
            return GetModId(mod, out_id);
        });
    }));
    register_core_api("bmlGetModVersion", BML_API_ID_bmlGetModVersion, reinterpret_cast<void *>(+[](BML_Mod mod, BML_Version *out_version) {
        return GuardResult("core.metadata", [mod, out_version] {
            return GetModVersion(mod, out_version);
        });
    }));
    register_core_api("bmlRegisterShutdownHook", BML_API_ID_bmlRegisterShutdownHook, reinterpret_cast<void *>(+[](BML_Mod mod, BML_ShutdownCallback callback, void *user_data) {
        return GuardResult("core.lifecycle", [mod, callback, user_data] {
            return RegisterShutdownHook(mod, callback, user_data);
        });
    }));
    register_core_api("bmlSetCurrentModule", BML_API_ID_bmlSetCurrentModule, reinterpret_cast<void *>(+[](BML_Mod mod) {
        return GuardResult("core.lifecycle", [mod] {
            return SetCurrentModule(mod);
        });
    }));
    register_core_api("bmlGetCurrentModule", BML_API_ID_bmlGetCurrentModule, reinterpret_cast<void *>(+[]() {
        return GetCurrentModuleHandle();
    }));
    register_core_api("bmlGetCoreCaps", BML_API_ID_bmlGetCoreCaps, reinterpret_cast<void *>(+[](BML_CoreCaps *out_caps) {
        return GuardResult("core.runtime", [out_caps] {
            return GetCoreCaps(out_caps);
        });
    }));

    // Error handling APIs (Task 1.1)
    register_core_api("bmlGetLastError", BML_API_ID_bmlGetLastError, reinterpret_cast<void *>(+[](BML_ErrorInfo *out_info) {
        return GetLastErrorInfo(out_info);
    }));
    register_core_api("bmlClearLastError", BML_API_ID_bmlClearLastError, reinterpret_cast<void *>(+[]() {
        ClearLastErrorInfo();
    }));
    register_core_api("bmlGetErrorString", BML_API_ID_bmlGetErrorString, reinterpret_cast<void *>(+[](BML_Result result) {
        return GetErrorString(result);
    }));

    registry.RegisterCoreApiSet(kCoreApiDescriptors, std::size(kCoreApiDescriptors));
}

} // namespace BML::Core
