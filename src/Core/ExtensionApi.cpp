#include "bml_extension.h"

#include "ApiRegistry.h"
#include "ApiRegistrationMacros.h"
#include "Context.h"
#include "ModHandle.h"
#include "CoreErrors.h"
#include "bml_api_ids.h"
#include "bml_capabilities.h"

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <mutex>
#include <atomic>

namespace BML::Core {

    // ========================================================================
    // Lifecycle Listener Storage
    // ========================================================================

    struct ListenerEntry {
        BML_ExtensionEventCallback callback;
        uint32_t event_mask;
        void* user_data;
    };

    static std::mutex s_listenerMutex;
    static std::unordered_map<uint64_t, ListenerEntry> s_listeners;
    static std::atomic<uint64_t> s_nextListenerId{1};

    static void NotifyListeners(BML_ExtensionEvent event, const BML_ExtensionInfo* info) {
        std::lock_guard<std::mutex> lock(s_listenerMutex);
        uint32_t eventBit = 1u << static_cast<uint32_t>(event);
        for (const auto& [id, entry] : s_listeners) {
            if (entry.event_mask == 0 || (entry.event_mask & eventBit)) {
                entry.callback(nullptr, event, info, entry.user_data);
            }
        }
    }

    // ========================================================================
    // Core Extension APIs
    // ========================================================================

    BML_Result BML_API_ExtensionRegister(const BML_ExtensionDesc* desc) {
        if (!desc || !desc->name || !desc->api_table || desc->api_size == 0) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        // Get current module to determine provider ID
        BML_Mod current_mod = Context::GetCurrentModule();
        if (!current_mod) {
            return BML_RESULT_INVALID_CONTEXT;
        }

        auto* mod_handle = Context::Instance().ResolveModHandle(current_mod);
        if (!mod_handle) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        // Register through ApiRegistry
        BML_ApiId id = ApiRegistry::Instance().RegisterExtension(
            desc->name,
            desc->version.major,
            desc->version.minor,
            desc->api_table,
            desc->api_size,
            mod_handle->id
        );

        if (id == BML_API_INVALID_ID) {
            return BML_RESULT_ALREADY_EXISTS;
        }

        // Notify listeners
        BML_ExtensionInfo info = BML_EXTENSION_INFO_INIT;
        info.name = desc->name;
        info.version = desc->version;
        info.description = desc->description;
        info.api_size = desc->api_size;
        info.capabilities = desc->capabilities;
        info.provider_id = mod_handle->id.c_str();
        NotifyListeners(BML_EXTENSION_EVENT_REGISTERED, &info);

        return BML_RESULT_OK;
    }

    BML_Result BML_API_ExtensionUnregister(const char* name) {
        if (!name) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        // Get current module to verify ownership
        BML_Mod current_mod = Context::GetCurrentModule();
        if (!current_mod) {
            return BML_RESULT_INVALID_CONTEXT;
        }

        auto* mod_handle = Context::Instance().ResolveModHandle(current_mod);
        if (!mod_handle) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        // Get extension info before unregistering for notification
        ApiRegistry::ApiMetadata meta;
        if (!ApiRegistry::Instance().TryGetMetadata(std::string(name), meta)) {
            return BML_RESULT_NOT_FOUND;
        }

        if (!meta.provider_mod || meta.provider_mod != mod_handle->id) {
            return BML_RESULT_PERMISSION_DENIED;
        }

        // Create info for notification
        BML_ExtensionInfo info = BML_EXTENSION_INFO_INIT;
        info.name = meta.name;
        info.version = bmlMakeVersion(meta.version_major, meta.version_minor, meta.version_patch);
        info.provider_id = meta.provider_mod;

        bool success = ApiRegistry::Instance().Unregister(name);
        if (success) {
            NotifyListeners(BML_EXTENSION_EVENT_UNREGISTERED, &info);
        }

        return success ? BML_RESULT_OK : BML_RESULT_NOT_FOUND;
    }

    BML_Result BML_API_ExtensionQuery(const char* name, BML_ExtensionInfo* out_info) {
        if (!name) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        ApiRegistry::ApiMetadata meta;
        bool found = ApiRegistry::Instance().TryGetMetadata(std::string(name), meta);

        if (!found) {
            return BML_RESULT_NOT_FOUND;
        }

        if (out_info) {
            out_info->struct_size = sizeof(BML_ExtensionInfo);
            out_info->name = meta.name;
            out_info->version = bmlMakeVersion(meta.version_major, meta.version_minor, meta.version_patch);
            out_info->provider_id = meta.provider_mod;
            out_info->description = meta.description;
            out_info->api_size = meta.api_size;
            out_info->state = BML_EXTENSION_STATE_ACTIVE;
        }

        return BML_RESULT_OK;
    }

    BML_Result BML_API_ExtensionLoad(
        const char* name,
        const BML_Version* required_version,
        void** out_api,
        BML_ExtensionInfo* out_info
    ) {
        if (!name || !out_api) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        uint32_t required_major = required_version ? required_version->major : 0;
        uint32_t required_minor = required_version ? required_version->minor : 0;

        const void* api = nullptr;
        uint32_t actual_major = 0, actual_minor = 0;

        bool compatible = ApiRegistry::Instance().LoadVersioned(
            name,
            required_major,
            required_minor,
            &api,
            &actual_major,
            &actual_minor
        );

        if (!compatible) {
            ApiRegistry::ApiMetadata meta;
            if (ApiRegistry::Instance().TryGetMetadata(std::string(name), meta)) {
                return BML_RESULT_VERSION_MISMATCH;
            }
            return BML_RESULT_NOT_FOUND;
        }

        *out_api = const_cast<void*>(api);

        if (out_info) {
            ApiRegistry::ApiMetadata meta;
            if (ApiRegistry::Instance().TryGetMetadata(std::string(name), meta)) {
                out_info->struct_size = sizeof(BML_ExtensionInfo);
                out_info->name = meta.name;
                out_info->version = bmlMakeVersion(meta.version_major, meta.version_minor, meta.version_patch);
                out_info->provider_id = meta.provider_mod;
                out_info->description = meta.description;
                out_info->api_size = meta.api_size;
                out_info->state = BML_EXTENSION_STATE_ACTIVE;
            }
        }

        return BML_RESULT_OK;
    }

    BML_Result BML_API_ExtensionEnumerate(
        const BML_ExtensionFilter* filter,
        BML_ExtensionEnumCallback callback,
        void* user_data
    ) {
        if (!callback) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        struct EnumContext {
            BML_ExtensionEnumCallback callback;
            void* user_data;
            const BML_ExtensionFilter* filter;
        };

        EnumContext ctx{callback, user_data, filter};

        ApiRegistry::Instance().Enumerate(
            [](BML_Context bmlCtx, const BML_ApiDescriptor* desc, void* raw_ctx) -> BML_Bool {
                auto* ctx = static_cast<EnumContext*>(raw_ctx);

                // Apply filter if specified
                if (ctx->filter) {
                    // Name pattern filter
                    if (ctx->filter->name_pattern) {
                        // Simple prefix match for now
                        // TODO: Implement proper glob matching
                        if (strstr(desc->name, ctx->filter->name_pattern) == nullptr) {
                            return BML_TRUE; // Skip, continue
                        }
                    }
                    // Version filter - check if extension version >= min_version
                    if (ctx->filter->min_version.major > 0 || ctx->filter->min_version.minor > 0) {
                        if (desc->version_major < ctx->filter->min_version.major) {
                            return BML_TRUE;
                        }
                        if (desc->version_major == ctx->filter->min_version.major &&
                            desc->version_minor < ctx->filter->min_version.minor) {
                            return BML_TRUE;
                        }
                    }
                }

                BML_ExtensionInfo info = BML_EXTENSION_INFO_INIT;
                info.name = desc->name;
                info.version = bmlMakeVersion(desc->version_major, desc->version_minor, 0);
                info.provider_id = desc->provider_mod;
                info.description = desc->description;

                return ctx->callback(bmlCtx, &info, ctx->user_data);
            },
            &ctx,
            BML_API_TYPE_EXTENSION
        );

        return BML_RESULT_OK;
    }

    BML_Result BML_API_ExtensionCount(const BML_ExtensionFilter* filter, uint32_t* out_count) {
        if (!out_count) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        if (!filter) {
            // Fast path: no filter
            size_t count = ApiRegistry::Instance().GetExtensionCount();
            *out_count = static_cast<uint32_t>(std::min<size_t>(
                count, static_cast<size_t>((std::numeric_limits<uint32_t>::max)())
            ));
            return BML_RESULT_OK;
        }

        // Count with filter
        uint32_t count = 0;
        BML_API_ExtensionEnumerate(filter,
            [](BML_Context, const BML_ExtensionInfo*, void* ud) -> BML_Bool {
                (*static_cast<uint32_t*>(ud))++;
                return BML_TRUE;
            },
            &count
        );
        *out_count = count;
        return BML_RESULT_OK;
    }

    // ========================================================================
    // Update APIs
    // ========================================================================

    BML_Result BML_API_ExtensionUpdateApi(const char* name, const void* api_table, size_t api_size) {
        if (!name || !api_table || api_size == 0) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        // Get current module to verify ownership
        BML_Mod current_mod = Context::GetCurrentModule();
        if (!current_mod) {
            return BML_RESULT_INVALID_CONTEXT;
        }

        auto* mod_handle = Context::Instance().ResolveModHandle(current_mod);
        if (!mod_handle) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        ApiRegistry::ApiMetadata meta;
        if (!ApiRegistry::Instance().TryGetMetadata(std::string(name), meta)) {
            return BML_RESULT_NOT_FOUND;
        }

        if (!meta.provider_mod || meta.provider_mod != mod_handle->id) {
            return BML_RESULT_PERMISSION_DENIED;
        }

        // TODO: Implement UpdateApiTable in ApiRegistry
        // For now, return NOT_SUPPORTED
        (void)api_table;
        (void)api_size;

        return BML_RESULT_NOT_SUPPORTED;
    }

    BML_Result BML_API_ExtensionDeprecate(const char* name, const char* replacement, const char* message) {
        if (!name) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        // Get current module to verify ownership
        BML_Mod current_mod = Context::GetCurrentModule();
        if (!current_mod) {
            return BML_RESULT_INVALID_CONTEXT;
        }

        auto* mod_handle = Context::Instance().ResolveModHandle(current_mod);
        if (!mod_handle) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        ApiRegistry::ApiMetadata meta;
        if (!ApiRegistry::Instance().TryGetMetadata(std::string(name), meta)) {
            return BML_RESULT_NOT_FOUND;
        }

        if (!meta.provider_mod || meta.provider_mod != mod_handle->id) {
            return BML_RESULT_PERMISSION_DENIED;
        }

        // TODO: Implement MarkDeprecated in ApiRegistry
        // For now, just notify listeners and return OK
        (void)replacement;
        (void)message;

        BML_ExtensionInfo info = BML_EXTENSION_INFO_INIT;
        info.name = name;
        info.version = bmlMakeVersion(meta.version_major, meta.version_minor, meta.version_patch);
        info.provider_id = meta.provider_mod;
        info.state = BML_EXTENSION_STATE_DEPRECATED;
        info.replacement_name = replacement;
        info.deprecation_message = message;
        NotifyListeners(BML_EXTENSION_EVENT_DEPRECATED, &info);

        return BML_RESULT_OK;
    }

    // ========================================================================
    // Lifecycle Listener APIs
    // ========================================================================

    BML_Result BML_API_ExtensionAddListener(
        BML_ExtensionEventCallback callback,
        uint32_t event_mask,
        void* user_data,
        uint64_t* out_id
    ) {
        if (!callback || !out_id) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        std::lock_guard<std::mutex> lock(s_listenerMutex);
        uint64_t id = s_nextListenerId++;
        s_listeners[id] = {callback, event_mask, user_data};
        *out_id = id;

        return BML_RESULT_OK;
    }

    BML_Result BML_API_ExtensionRemoveListener(uint64_t id) {
        std::lock_guard<std::mutex> lock(s_listenerMutex);
        auto it = s_listeners.find(id);
        if (it == s_listeners.end()) {
            return BML_RESULT_NOT_FOUND;
        }
        s_listeners.erase(it);
        return BML_RESULT_OK;
    }

    // ========================================================================
    // Capability Query
    // ========================================================================

    BML_Result BML_API_GetExtensionCaps(BML_ExtensionCaps* out_caps) {
        if (!out_caps) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        BML_ExtensionCaps caps = BML_EXTENSION_CAPS_INIT;
        caps.api_version = bmlGetApiVersion();
        caps.capability_flags =
            BML_EXTENSION_CAP_REGISTER |
            BML_EXTENSION_CAP_QUERY |
            BML_EXTENSION_CAP_LOAD |
            BML_EXTENSION_CAP_ENUMERATE |
            BML_EXTENSION_CAP_UNREGISTER |
            BML_EXTENSION_CAP_UPDATE |
            BML_EXTENSION_CAP_LIFECYCLE |
            BML_EXTENSION_CAP_FILTER;

        size_t count = ApiRegistry::Instance().GetExtensionCount();
        caps.registered_count = static_cast<uint32_t>(std::min<size_t>(
            count, static_cast<size_t>((std::numeric_limits<uint32_t>::max)())
        ));
        caps.max_extensions = 0; // Unlimited

        *out_caps = caps;
        return BML_RESULT_OK;
    }

    // ========================================================================
    // Registration
    // ========================================================================

    void RegisterExtensionApis() {
        BML_BEGIN_API_REGISTRATION();

        // Core Extension APIs
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlExtensionRegister, "extension", BML_API_ExtensionRegister, BML_CAP_EXTENSION_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlExtensionUnregister, "extension", BML_API_ExtensionUnregister, BML_CAP_EXTENSION_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlExtensionQuery, "extension", BML_API_ExtensionQuery, BML_CAP_EXTENSION_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlExtensionLoad, "extension", BML_API_ExtensionLoad, BML_CAP_EXTENSION_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlExtensionEnumerate, "extension", BML_API_ExtensionEnumerate, BML_CAP_EXTENSION_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlExtensionCount, "extension", BML_API_ExtensionCount, BML_CAP_EXTENSION_BASIC);

        // Update APIs
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlExtensionUpdateApi, "extension", BML_API_ExtensionUpdateApi, BML_CAP_EXTENSION_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlExtensionDeprecate, "extension", BML_API_ExtensionDeprecate, BML_CAP_EXTENSION_BASIC);

        // Lifecycle APIs
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlExtensionAddListener, "extension", BML_API_ExtensionAddListener, BML_CAP_EXTENSION_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlExtensionRemoveListener, "extension", BML_API_ExtensionRemoveListener, BML_CAP_EXTENSION_BASIC);

        // Capability Query
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlGetExtensionCaps, "extension", BML_API_GetExtensionCaps, BML_CAP_EXTENSION_BASIC);
    }

} // namespace BML::Core
