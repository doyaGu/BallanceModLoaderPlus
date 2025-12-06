#include "bml_extension.h"

#include "ApiRegistry.h"
#include "ApiRegistrationMacros.h"
#include "Context.h"
#include "Logging.h"
#include "ModHandle.h"
#include "CoreErrors.h"
#include "bml_api_ids.h"
#include "bml_capabilities.h"

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <cstring>
#include <string>

namespace BML::Core {
    namespace {
        constexpr char kExtensionLogCategory[] = "extension";

        constexpr size_t kExtensionDescMinSize = sizeof(BML_ExtensionDesc);
        constexpr size_t kExtensionInfoMinSize = sizeof(BML_ExtensionInfo);
        constexpr size_t kExtensionCapsMinSize = sizeof(BML_ExtensionCaps);
        constexpr size_t kExtensionFilterMinSize = sizeof(BML_ExtensionFilter);

        bool HasValidExtensionDesc(const BML_ExtensionDesc *desc) {
            return desc && desc->struct_size >= kExtensionDescMinSize;
        }

        bool HasValidExtensionInfo(const BML_ExtensionInfo *info) {
            return info && info->struct_size >= kExtensionInfoMinSize;
        }

        bool HasValidExtensionCaps(const BML_ExtensionCaps *caps) {
            return caps && caps->struct_size >= kExtensionCapsMinSize;
        }

        bool HasValidExtensionFilter(const BML_ExtensionFilter *filter) {
            return filter && filter->struct_size >= kExtensionFilterMinSize;
        }

        void PopulateExtensionInfo(const ApiRegistry::ApiMetadata &meta,
                                   BML_ExtensionInfo *out_info,
                                   BML_ExtensionState state = BML_EXTENSION_STATE_ACTIVE) {
            if (!out_info) {
                return;
            }

            BML_ExtensionInfo info = BML_EXTENSION_INFO_INIT;
            info.name = meta.name;
            info.provider_id = meta.provider_mod;
            info.version = bmlMakeVersion(meta.version_major, meta.version_minor, meta.version_patch);
            info.state = state;
            info.description = meta.description;
            info.api_size = meta.api_size;
            info.capabilities = meta.capabilities;
            *out_info = info;
        }
    }

    // ========================================================================
    // Simple Glob Pattern Matching
    // ========================================================================

    /**
     * @brief Match a string against a glob pattern
     * Supports: * (any chars), ? (single char)
     */
    static bool GlobMatch(const char *pattern, const char *str) {
        if (!pattern || !str) return false;

        while (*pattern && *str) {
            if (*pattern == '*') {
                // Skip consecutive asterisks
                while (*pattern == '*') pattern++;
                if (!*pattern) return true; // Trailing * matches everything

                // Try matching rest of pattern at each position
                while (*str) {
                    if (GlobMatch(pattern, str)) return true;
                    str++;
                }
                return false;
            } else if (*pattern == '?' || *pattern == *str) {
                pattern++;
                str++;
            } else {
                return false;
            }
        }

        // Skip trailing asterisks
        while (*pattern == '*') pattern++;
        return *pattern == '\0' && *str == '\0';
    }

    // ========================================================================
    // Lifecycle Listener Storage
    // ========================================================================

    struct ListenerEntry {
        BML_ExtensionEventCallback callback;
        uint32_t event_mask;
        void *user_data;
    };

    static std::mutex s_listenerMutex;
    static std::unordered_map<uint64_t, ListenerEntry> s_listeners;
    static std::atomic<uint64_t> s_nextListenerId{1};

    static void NotifyListeners(BML_ExtensionEvent event, const BML_ExtensionInfo *info) {
        std::lock_guard<std::mutex> lock(s_listenerMutex);
        uint32_t eventBit = 1u << static_cast<uint32_t>(event);
        for (const auto &[id, entry] : s_listeners) {
            if (entry.event_mask == 0 || (entry.event_mask & eventBit)) {
                entry.callback(nullptr, event, info, entry.user_data);
            }
        }
    }

    // ========================================================================
    // Extension Reference Counting
    // ========================================================================

    static std::mutex s_refCountMutex;
    static std::unordered_map<std::string, uint32_t> s_extensionRefCounts;

    static void IncrementRefCount(const std::string &name) {
        std::lock_guard<std::mutex> lock(s_refCountMutex);
        s_extensionRefCounts[name]++;
    }

    static bool DecrementRefCount(const std::string &name) {
        std::lock_guard<std::mutex> lock(s_refCountMutex);
        auto it = s_extensionRefCounts.find(name);
        if (it == s_extensionRefCounts.end() || it->second == 0) {
            return false;
        }
        it->second--;
        if (it->second == 0) {
            s_extensionRefCounts.erase(it);
        }
        return true;
    }

    static uint32_t GetRefCountInternal(const std::string &name) {
        std::lock_guard<std::mutex> lock(s_refCountMutex);
        auto it = s_extensionRefCounts.find(name);
        return (it != s_extensionRefCounts.end()) ? it->second : 0;
    }

    // ========================================================================
    // Core Extension APIs
    // ========================================================================

    BML_Result BML_API_ExtensionRegister(const BML_ExtensionDesc *desc) {
        if (!HasValidExtensionDesc(desc) || !desc->name || !desc->api_table || desc->api_size == 0) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        // Get current module to determine provider ID
        BML_Mod current_mod = Context::GetCurrentModule();
        if (!current_mod) {
            return BML_RESULT_INVALID_CONTEXT;
        }

        auto *mod_handle = Context::Instance().ResolveModHandle(current_mod);
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
            CoreLog(BML_LOG_WARN, kExtensionLogCategory,
                    "Failed to register extension '%s' (already exists or ID exhausted)",
                    desc->name);
            return BML_RESULT_ALREADY_EXISTS;
        }

        CoreLog(BML_LOG_INFO, kExtensionLogCategory,
                "Registered extension '%s' v%u.%u by provider '%s'",
                desc->name, desc->version.major, desc->version.minor,
                mod_handle->id.c_str());

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

    BML_Result BML_API_ExtensionUnregister(const char *name) {
        if (!name) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        // Check reference count - cannot unregister if still in use
        uint32_t ref_count = GetRefCountInternal(std::string(name));
        if (ref_count > 0) {
            CoreLog(BML_LOG_WARN, kExtensionLogCategory,
                    "Cannot unregister extension '%s': still in use (refcount=%u)",
                    name, ref_count);
            return BML_RESULT_EXTENSION_IN_USE;
        }

        // Get current module to verify ownership
        BML_Mod current_mod = Context::GetCurrentModule();
        if (!current_mod) {
            return BML_RESULT_INVALID_CONTEXT;
        }

        auto *mod_handle = Context::Instance().ResolveModHandle(current_mod);
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
            CoreLog(BML_LOG_INFO, kExtensionLogCategory,
                    "Unregistered extension '%s'", name);
            NotifyListeners(BML_EXTENSION_EVENT_UNREGISTERED, &info);
        }

        return success ? BML_RESULT_OK : BML_RESULT_NOT_FOUND;
    }

    BML_Result BML_API_ExtensionQuery(const char *name, BML_ExtensionInfo *out_info) {
        if (!name) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        if (out_info && !HasValidExtensionInfo(out_info)) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        ApiRegistry::ApiMetadata meta;
        bool found = ApiRegistry::Instance().TryGetMetadata(std::string(name), meta);

        if (!found) {
            return BML_RESULT_NOT_FOUND;
        }

        if (out_info) {
            PopulateExtensionInfo(meta, out_info);
        }

        return BML_RESULT_OK;
    }

    BML_Result BML_API_ExtensionLoad(
        const char *name,
        const BML_Version *required_version,
        void **out_api,
        BML_ExtensionInfo *out_info
    ) {
        if (!name || !out_api || (out_info && !HasValidExtensionInfo(out_info))) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        uint32_t required_major = required_version ? required_version->major : 0;
        uint32_t required_minor = required_version ? required_version->minor : 0;

        const void *api = nullptr;
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

        *out_api = const_cast<void *>(api);

        // Increment reference count on successful load
        IncrementRefCount(std::string(name));

        if (out_info) {
            ApiRegistry::ApiMetadata meta;
            if (ApiRegistry::Instance().TryGetMetadata(std::string(name), meta)) {
                PopulateExtensionInfo(meta, out_info);
            }
        }

        return BML_RESULT_OK;
    }

    BML_Result BML_API_ExtensionUnload(const char *name) {
        if (!name) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        // Check if extension exists
        ApiRegistry::ApiMetadata meta;
        if (!ApiRegistry::Instance().TryGetMetadata(std::string(name), meta)) {
            return BML_RESULT_NOT_FOUND;
        }

        // Decrement reference count
        if (!DecrementRefCount(std::string(name))) {
            return BML_RESULT_FAIL; // Not loaded by this module
        }

        return BML_RESULT_OK;
    }

    BML_Result BML_API_ExtensionGetRefCount(const char *name, uint32_t *out_count) {
        if (!name || !out_count) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        // Check if extension exists
        ApiRegistry::ApiMetadata meta;
        if (!ApiRegistry::Instance().TryGetMetadata(std::string(name), meta)) {
            return BML_RESULT_NOT_FOUND;
        }

        *out_count = GetRefCountInternal(std::string(name));
        return BML_RESULT_OK;
    }

    BML_Result BML_API_ExtensionEnumerate(
        const BML_ExtensionFilter *filter,
        BML_ExtensionEnumCallback callback,
        void *user_data
    ) {
        if (!callback) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        if (filter && !HasValidExtensionFilter(filter)) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        struct EnumContext {
            BML_ExtensionEnumCallback callback;
            void *user_data;
            const BML_ExtensionFilter *filter;
        };

        EnumContext ctx{callback, user_data, filter};

        ApiRegistry::Instance().Enumerate(
            [](BML_Context bmlCtx, const BML_ApiDescriptor *desc, void *raw_ctx) -> BML_Bool {
                auto *ctx = static_cast<EnumContext *>(raw_ctx);

                ApiRegistry::ApiMetadata meta{};
                bool have_meta = desc && desc->name &&
                    ApiRegistry::Instance().TryGetMetadata(std::string(desc->name), meta);
                if (!have_meta && desc) {
                    meta.name = desc->name;
                    meta.provider_mod = desc->provider_mod;
                    meta.version_major = static_cast<uint16_t>(desc->version_major);
                    meta.version_minor = static_cast<uint16_t>(desc->version_minor);
                    meta.version_patch = static_cast<uint16_t>(desc->version_patch);
                    meta.capabilities = desc->capabilities;
                    meta.description = desc->description;
                    meta.api_size = 0;
                }

                const BML_ExtensionFilter *filter = ctx->filter;
                if (filter && desc) {
                    if (filter->name_pattern && !GlobMatch(filter->name_pattern, desc->name)) {
                        return BML_TRUE;
                    }
                    if (filter->provider_pattern) {
                        const char *provider = meta.provider_mod ? meta.provider_mod : "";
                        if (!GlobMatch(filter->provider_pattern, provider)) {
                            return BML_TRUE;
                        }
                    }

                    const auto &min = filter->min_version;
                    if (min.major || min.minor || min.patch) {
                        if (meta.version_major < min.major) {
                            return BML_TRUE;
                        }
                        if (meta.version_major == min.major && meta.version_minor < min.minor) {
                            return BML_TRUE;
                        }
                        if (meta.version_major == min.major && meta.version_minor == min.minor &&
                            meta.version_patch < min.patch) {
                            return BML_TRUE;
                        }
                    }

                    const auto &max = filter->max_version;
                    if (max.major || max.minor || max.patch) {
                        if (meta.version_major > max.major) {
                            return BML_TRUE;
                        }
                        if (meta.version_major == max.major && meta.version_minor > max.minor) {
                            return BML_TRUE;
                        }
                        if (meta.version_major == max.major && meta.version_minor == max.minor &&
                            meta.version_patch > max.patch) {
                            return BML_TRUE;
                        }
                    }

                    if (filter->required_caps &&
                        (meta.capabilities & filter->required_caps) != filter->required_caps) {
                        return BML_TRUE;
                    }

                    if (filter->include_states) {
                        constexpr uint32_t kActiveBit = 1u << static_cast<uint32_t>(BML_EXTENSION_STATE_ACTIVE);
                        if ((filter->include_states & kActiveBit) == 0) {
                            return BML_TRUE;
                        }
                    }
                }

                BML_ExtensionInfo info = BML_EXTENSION_INFO_INIT;
                PopulateExtensionInfo(meta, &info);
                return ctx->callback(bmlCtx, &info, ctx->user_data);
            },
            &ctx,
            BML_API_TYPE_EXTENSION
        );

        return BML_RESULT_OK;
    }

    BML_Result BML_API_ExtensionCount(const BML_ExtensionFilter *filter, uint32_t *out_count) {
        if (!out_count) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        if (filter && !HasValidExtensionFilter(filter)) {
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
        BML_Result enumerate_result = BML_API_ExtensionEnumerate(
            filter,
            [](BML_Context, const BML_ExtensionInfo *, void *ud) -> BML_Bool {
                (*static_cast<uint32_t *>(ud))++;
                return BML_TRUE;
            },
            &count
        );
        if (enumerate_result != BML_RESULT_OK) {
            return enumerate_result;
        }
        *out_count = count;
        return BML_RESULT_OK;
    }

    // ========================================================================
    // Update APIs
    // ========================================================================

    BML_Result BML_API_ExtensionUpdateApi(const char *name, const void *api_table, size_t api_size) {
        if (!name || !api_table || api_size == 0) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        // Get current module to verify ownership
        BML_Mod current_mod = Context::GetCurrentModule();
        if (!current_mod) {
            return BML_RESULT_INVALID_CONTEXT;
        }

        auto *mod_handle = Context::Instance().ResolveModHandle(current_mod);
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

        bool success = ApiRegistry::Instance().UpdateApiTable(name, api_table, api_size);
        if (!success) {
            return BML_RESULT_FAIL;
        }

        // Notify listeners of API update
        BML_ExtensionInfo info = BML_EXTENSION_INFO_INIT;
        info.name = name;
        info.version = bmlMakeVersion(meta.version_major, meta.version_minor, meta.version_patch);
        info.provider_id = meta.provider_mod;
        info.api_size = api_size;
        NotifyListeners(BML_EXTENSION_EVENT_REGISTERED, &info); // Re-use registered event

        return BML_RESULT_OK;
    }

    BML_Result BML_API_ExtensionDeprecate(const char *name, const char *replacement, const char *message) {
        if (!name) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        // Get current module to verify ownership
        BML_Mod current_mod = Context::GetCurrentModule();
        if (!current_mod) {
            return BML_RESULT_INVALID_CONTEXT;
        }

        auto *mod_handle = Context::Instance().ResolveModHandle(current_mod);
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

        bool success = ApiRegistry::Instance().MarkDeprecated(name, replacement, message);
        if (!success) {
            return BML_RESULT_FAIL;
        }

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
        void *user_data,
        uint64_t *out_id
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

    BML_Result BML_API_ExtensionGetCaps(BML_ExtensionCaps *out_caps) {
        if (!HasValidExtensionCaps(out_caps)) {
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
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlExtensionUnload, "extension", BML_API_ExtensionUnload, BML_CAP_EXTENSION_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlExtensionGetRefCount, "extension", BML_API_ExtensionGetRefCount, BML_CAP_EXTENSION_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlExtensionEnumerate, "extension", BML_API_ExtensionEnumerate, BML_CAP_EXTENSION_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlExtensionCount, "extension", BML_API_ExtensionCount, BML_CAP_EXTENSION_BASIC);

        // Update APIs
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlExtensionUpdateApi, "extension", BML_API_ExtensionUpdateApi, BML_CAP_EXTENSION_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlExtensionDeprecate, "extension", BML_API_ExtensionDeprecate, BML_CAP_EXTENSION_BASIC);

        // Lifecycle APIs
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlExtensionAddListener, "extension", BML_API_ExtensionAddListener, BML_CAP_EXTENSION_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlExtensionRemoveListener, "extension", BML_API_ExtensionRemoveListener, BML_CAP_EXTENSION_BASIC);

        // Capability Query
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlExtensionGetCaps, "extension", BML_API_ExtensionGetCaps, BML_CAP_EXTENSION_BASIC);
    }
} // namespace BML::Core
