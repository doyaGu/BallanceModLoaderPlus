#include "ApiRegistration.h"
#include "ApiRegistry.h"
#include "ApiRegistrationMacros.h"

#include <algorithm>
#include <iterator>
#include <variant>

#include "bml_core.h"
#include "bml_version.h"

#include "Context.h"
#include "ModHandle.h"
#include "ModuleLifecycle.h"
#include "CoreErrors.h"

namespace BML::Core {
    namespace {
        // ============================================================================
        // Helper Functions
        // ============================================================================

        Context *FromHandle(BML_Context ctx) {
            return Context::FromHandle(ctx);
        }

        Context *FromModHandle(BML_Mod mod) {
            return Context::ContextFromMod(mod);
        }

        BML_Mod_T *ResolveExplicitMod(Context &context, BML_Mod mod) {
            if (!mod) {
                return nullptr;
            }
            return context.ResolveModHandle(mod);
        }

        // ============================================================================
        // Core API Subsystem Descriptors
        // ============================================================================

        enum CoreApiMask : uint32_t {
            CORE_API_LOGGING    = 1u << 0,
            CORE_API_CONFIG     = 1u << 1,
            CORE_API_IMC        = 1u << 2,
            CORE_API_RESOURCE   = 1u << 3,
            CORE_API_MEMORY     = 1u << 4,
            CORE_API_DIAGNOSTIC = 1u << 5,
            CORE_API_SYNC       = 1u << 6,
            CORE_API_PROFILING  = 1u << 7,
            CORE_API_TRACING    = 1u << 8,
            CORE_API_INTERFACE  = 1u << 9,
            CORE_API_TIMER      = 1u << 10,
            CORE_API_HOOK       = 1u << 11,
            CORE_API_LOCALE     = 1u << 12,
        };

        constexpr ApiRegistry::CoreApiDescriptor kCoreApiDescriptors[] = {
            {"Logging", RegisterLoggingApis, CORE_API_LOGGING, 0u},
            {"ConfigStore", RegisterConfigApis, CORE_API_CONFIG, CORE_API_LOGGING},
            {"ImcBus", RegisterImcApis, CORE_API_IMC, CORE_API_LOGGING},
            {"Resource", RegisterResourceApis, CORE_API_RESOURCE, CORE_API_LOGGING},
            {"Interface", RegisterInterfaceApis, CORE_API_INTERFACE, CORE_API_RESOURCE},
            {"Memory", RegisterMemoryApis, CORE_API_MEMORY, 0u},
            {"Diagnostic", RegisterDiagnosticApis, CORE_API_DIAGNOSTIC, 0u},
            {"Sync", RegisterSyncApis, CORE_API_SYNC, 0u},
            {"Profiling", RegisterProfilingApis, CORE_API_PROFILING, 0u},
            {"Tracing", RegisterTracingApis, CORE_API_TRACING, CORE_API_LOGGING},
            {"Timer", RegisterTimerApis, CORE_API_TIMER, 0u},
            {"Locale", RegisterLocaleApis, CORE_API_LOCALE, 0u},
            {"Hook", RegisterHookApis, CORE_API_HOOK, 0u},
        };
    } // namespace

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

    BML_Result BML_API_ContextSetUserData(BML_Context ctx, const char *key, void *data, BML_UserDataDestructor destructor) {
        auto *context = FromHandle(ctx);
        if (!context)
            return BML_RESULT_INVALID_HANDLE;
        if (!key)
            return BML_RESULT_INVALID_ARGUMENT;
        return context->SetUserData(key, data, destructor);
    }

    BML_Result BML_API_ContextGetUserData(BML_Context ctx, const char *key, void **out_data) {
        auto *context = FromHandle(ctx);
        if (!context)
            return BML_RESULT_INVALID_HANDLE;
        if (!key || !out_data)
            return BML_RESULT_INVALID_ARGUMENT;
        return context->GetUserData(key, out_data);
    }

        BML_Result BML_API_RequestCapability(BML_Mod mod, const char *capability_id) {
        if (!capability_id)
            return BML_RESULT_INVALID_ARGUMENT;
        auto *context = FromModHandle(mod);
        if (!context)
            return BML_RESULT_INVALID_ARGUMENT;
        auto *handle = ResolveExplicitMod(*context, mod);
        if (!handle)
            return BML_RESULT_INVALID_ARGUMENT;
        auto it = std::find(handle->capabilities.begin(), handle->capabilities.end(), capability_id);
        return it != handle->capabilities.end() ? BML_RESULT_OK : BML_RESULT_NOT_FOUND;
    }

    BML_Result BML_API_CheckCapability(BML_Mod mod, const char *capability_id, BML_Bool *out_supported) {
        if (!capability_id || !out_supported)
            return BML_RESULT_INVALID_ARGUMENT;
        *out_supported = BML_FALSE;
        auto *context = FromModHandle(mod);
        if (!context)
            return BML_RESULT_INVALID_ARGUMENT;
        auto *handle = ResolveExplicitMod(*context, mod);
        if (!handle)
            return BML_RESULT_INVALID_ARGUMENT;
        bool supported = std::find(handle->capabilities.begin(), handle->capabilities.end(), capability_id) !=
            handle->capabilities.end();
        *out_supported = supported ? BML_TRUE : BML_FALSE;
        return BML_RESULT_OK;
    }

    BML_Result BML_API_GetModId(BML_Mod mod, const char **out_id) {
        if (!out_id)
            return BML_RESULT_INVALID_ARGUMENT;
        auto *context = FromModHandle(mod);
        if (!context)
            return BML_RESULT_INVALID_ARGUMENT;
        auto *handle = ResolveExplicitMod(*context, mod);
        if (!handle)
            return BML_RESULT_INVALID_ARGUMENT;
        *out_id = handle->id.c_str();
        return BML_RESULT_OK;
    }

    BML_Result BML_API_GetModVersion(BML_Mod mod, BML_Version *out_version) {
        if (!out_version)
            return BML_RESULT_INVALID_ARGUMENT;
        auto *context = FromModHandle(mod);
        if (!context)
            return BML_RESULT_INVALID_ARGUMENT;
        auto *handle = ResolveExplicitMod(*context, mod);
        if (!handle)
            return BML_RESULT_INVALID_ARGUMENT;
        *out_version = handle->version;
        return BML_RESULT_OK;
    }

    BML_Result BML_API_RegisterShutdownHook(BML_Mod owner,
                                            BML_ShutdownCallback callback,
                                            void *user_data) {
        if (!callback)
            return BML_RESULT_INVALID_ARGUMENT;
        auto *context = FromModHandle(owner);
        if (!context)
            return BML_RESULT_INVALID_ARGUMENT;
        auto *handle = ResolveExplicitMod(*context, owner);
        if (!handle)
            return BML_RESULT_INVALID_ARGUMENT;
        context->AppendShutdownHook(handle, callback, user_data);
        return BML_RESULT_OK;
    }

    BML_Result BML_API_CleanupModuleState(BML_Mod mod) {
        auto *kernel = Context::KernelFromMod(mod);
        auto *context = Context::ContextFromMod(mod);
        if (!kernel || !context) {
            return BML_RESULT_INVALID_CONTEXT;
        }

        auto *handle = context->ResolveModHandle(mod);
        if (!handle) {
            return BML_RESULT_INVALID_HANDLE;
        }

        CleanupModuleKernelState(*kernel, handle->id, mod);
        return BML_RESULT_OK;
    }

    // -- Manifest custom field access --

    template <typename T>
    BML_Result ResolveManifestField(BML_Mod_T *handle, const char *path, const T **out) {
        if (!path) return BML_RESULT_INVALID_ARGUMENT;
        if (!handle || !handle->manifest) return BML_RESULT_INVALID_HANDLE;
        auto it = handle->manifest->custom_fields.find(path);
        if (it == handle->manifest->custom_fields.end()) return BML_RESULT_NOT_FOUND;
        auto *val = std::get_if<T>(&it->second);
        if (!val) return BML_RESULT_CONFIG_TYPE_MISMATCH;
        *out = val;
        return BML_RESULT_OK;
    }

    BML_Result BML_API_GetManifestString(BML_Mod mod, const char *path, const char **out_value) {
        if (!out_value) return BML_RESULT_INVALID_ARGUMENT;
        auto *context = FromModHandle(mod);
        if (!context) return BML_RESULT_INVALID_ARGUMENT;
        const std::string *str = nullptr;
        BML_Result r = ResolveManifestField(ResolveExplicitMod(*context, mod), path, &str);
        if (r == BML_RESULT_OK) *out_value = str->c_str();
        return r;
    }

    BML_Result BML_API_GetManifestInt(BML_Mod mod, const char *path, int64_t *out_value) {
        if (!out_value) return BML_RESULT_INVALID_ARGUMENT;
        auto *context = FromModHandle(mod);
        if (!context) return BML_RESULT_INVALID_ARGUMENT;
        const int64_t *val = nullptr;
        BML_Result r = ResolveManifestField(ResolveExplicitMod(*context, mod), path, &val);
        if (r == BML_RESULT_OK) *out_value = *val;
        return r;
    }

    BML_Result BML_API_GetManifestFloat(BML_Mod mod, const char *path, double *out_value) {
        if (!out_value) return BML_RESULT_INVALID_ARGUMENT;
        auto *context = FromModHandle(mod);
        if (!context) return BML_RESULT_INVALID_ARGUMENT;
        const double *val = nullptr;
        BML_Result r = ResolveManifestField(ResolveExplicitMod(*context, mod), path, &val);
        if (r == BML_RESULT_OK) *out_value = *val;
        return r;
    }

    BML_Result BML_API_GetManifestBool(BML_Mod mod, const char *path, BML_Bool *out_value) {
        if (!out_value) return BML_RESULT_INVALID_ARGUMENT;
        auto *context = FromModHandle(mod);
        if (!context) return BML_RESULT_INVALID_ARGUMENT;
        const bool *val = nullptr;
        BML_Result r = ResolveManifestField(ResolveExplicitMod(*context, mod), path, &val);
        if (r == BML_RESULT_OK) *out_value = *val ? BML_TRUE : BML_FALSE;
        return r;
    }

    // -- Extended module metadata APIs --

    BML_Result BML_API_GetModDirectory(BML_Mod mod, const char **out_dir) {
        if (!out_dir) return BML_RESULT_INVALID_ARGUMENT;
        auto *context = FromModHandle(mod);
        if (!context) return BML_RESULT_INVALID_ARGUMENT;
        auto *handle = ResolveExplicitMod(*context, mod);
        if (!handle || !handle->manifest) return BML_RESULT_INVALID_ARGUMENT;
        *out_dir = handle->directory_utf8.c_str();
        return BML_RESULT_OK;
    }

    BML_Result BML_API_GetModAssetMount(BML_Mod mod, const char **out_mount) {
        if (!out_mount) return BML_RESULT_INVALID_ARGUMENT;
        auto *context = FromModHandle(mod);
        if (!context) return BML_RESULT_INVALID_ARGUMENT;
        auto *handle = ResolveExplicitMod(*context, mod);
        if (!handle || !handle->manifest) return BML_RESULT_INVALID_ARGUMENT;
        *out_mount = handle->manifest->assets.mount.empty()
            ? nullptr
            : handle->manifest->assets.mount.c_str();
        return BML_RESULT_OK;
    }

    BML_Result BML_API_GetModName(BML_Mod mod, const char **out_name) {
        if (!out_name) return BML_RESULT_INVALID_ARGUMENT;
        auto *context = FromModHandle(mod);
        if (!context) return BML_RESULT_INVALID_ARGUMENT;
        auto *handle = ResolveExplicitMod(*context, mod);
        if (!handle || !handle->manifest) return BML_RESULT_INVALID_ARGUMENT;
        *out_name = handle->manifest->package.name.c_str();
        return BML_RESULT_OK;
    }

    BML_Result BML_API_GetModDescription(BML_Mod mod, const char **out_desc) {
        if (!out_desc) return BML_RESULT_INVALID_ARGUMENT;
        auto *context = FromModHandle(mod);
        if (!context) return BML_RESULT_INVALID_ARGUMENT;
        auto *handle = ResolveExplicitMod(*context, mod);
        if (!handle || !handle->manifest) return BML_RESULT_INVALID_ARGUMENT;
        *out_desc = handle->manifest->package.description.c_str();
        return BML_RESULT_OK;
    }

    BML_Result BML_API_GetModAuthorCount(BML_Mod mod, uint32_t *out_count) {
        if (!out_count) return BML_RESULT_INVALID_ARGUMENT;
        auto *context = FromModHandle(mod);
        if (!context) return BML_RESULT_INVALID_ARGUMENT;
        auto *handle = ResolveExplicitMod(*context, mod);
        if (!handle || !handle->manifest) return BML_RESULT_INVALID_ARGUMENT;
        *out_count = static_cast<uint32_t>(handle->manifest->package.authors.size());
        return BML_RESULT_OK;
    }

    BML_Result BML_API_GetModAuthorAt(BML_Mod mod, uint32_t index, const char **out_author) {
        if (!out_author) return BML_RESULT_INVALID_ARGUMENT;
        auto *context = FromModHandle(mod);
        if (!context) return BML_RESULT_INVALID_ARGUMENT;
        auto *handle = ResolveExplicitMod(*context, mod);
        if (!handle || !handle->manifest) return BML_RESULT_INVALID_ARGUMENT;
        if (index >= handle->manifest->package.authors.size()) return BML_RESULT_INVALID_ARGUMENT;
        *out_author = handle->manifest->package.authors[index].c_str();
        return BML_RESULT_OK;
    }

    void RegisterCoreApis(ApiRegistry &apiRegistry) {
        BML_BEGIN_API_REGISTRATION(apiRegistry);

        // Context management APIs
        BML_REGISTER_API_GUARDED(bmlContextRetain, "core.context", BML_API_ContextRetain);
        BML_REGISTER_API_GUARDED(bmlContextRelease, "core.context", BML_API_ContextRelease);
        BML_REGISTER_API_GUARDED(bmlContextSetUserData, "core.context", BML_API_ContextSetUserData);
        BML_REGISTER_API_GUARDED(bmlContextGetUserData, "core.context", BML_API_ContextGetUserData);
        // Capability APIs
        BML_REGISTER_API_GUARDED(bmlRequestCapability, "core.capabilities", BML_API_RequestCapability);
        BML_REGISTER_API_GUARDED(bmlCheckCapability, "core.capabilities", BML_API_CheckCapability);

        // Module metadata APIs
        BML_REGISTER_API_GUARDED(bmlGetModId, "core.metadata", BML_API_GetModId);
        BML_REGISTER_API_GUARDED(bmlGetModVersion, "core.metadata", BML_API_GetModVersion);
        BML_REGISTER_API_GUARDED(bmlGetModDirectory, "core.metadata", BML_API_GetModDirectory);
        BML_REGISTER_API_GUARDED(bmlGetModAssetMount, "core.metadata", BML_API_GetModAssetMount);
        BML_REGISTER_API_GUARDED(bmlGetModName, "core.metadata", BML_API_GetModName);
        BML_REGISTER_API_GUARDED(bmlGetModDescription, "core.metadata", BML_API_GetModDescription);
        BML_REGISTER_API_GUARDED(bmlGetModAuthorCount, "core.metadata", BML_API_GetModAuthorCount);
        BML_REGISTER_API_GUARDED(bmlGetModAuthorAt, "core.metadata", BML_API_GetModAuthorAt);

        // Manifest custom field access APIs
        BML_REGISTER_API_GUARDED(bmlGetManifestString, "core.manifest", BML_API_GetManifestString);
        BML_REGISTER_API_GUARDED(bmlGetManifestInt, "core.manifest", BML_API_GetManifestInt);
        BML_REGISTER_API_GUARDED(bmlGetManifestFloat, "core.manifest", BML_API_GetManifestFloat);
        BML_REGISTER_API_GUARDED(bmlGetManifestBool, "core.manifest", BML_API_GetManifestBool);

        // Lifecycle APIs
        BML_REGISTER_API_GUARDED(
            bmlRegisterShutdownHook, "core.lifecycle", BML_API_RegisterShutdownHook);

        // Register all subsystem APIs in dependency order
        registry.RegisterCoreApiSet(kCoreApiDescriptors, std::size(kCoreApiDescriptors));
    }
} // namespace BML::Core
