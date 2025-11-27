#include "ApiRegistry.h"

#include <cstring>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "Context.h"

namespace BML::Core {

namespace {
    void DebugWarning(const char *message) {
        std::string line = "[BML ApiRegistry] WARNING: ";
        line += message;
        line += "\n";
        OutputDebugStringA(line.c_str());
    }
    
    void DebugInfo(const char *message) {
        std::string line = "[BML ApiRegistry] INFO: ";
        line += message;
        line += "\n";
        OutputDebugStringA(line.c_str());
    }
}

namespace {
    std::shared_mutex g_Mutex;
    
    // Version counter for cache invalidation
    // Incremented whenever APIs are unregistered or cleared
    std::atomic<uint64_t> g_CacheVersion{0};
    
    // Thread-local cache for hot API lookups
    thread_local ApiRegistry::CacheEntry g_TlsCache[TLS_CACHE_SIZE];
    thread_local size_t g_TlsCacheNextSlot = 0;
    thread_local uint64_t g_TlsCacheVersion = 0;  // Local copy of g_CacheVersion
}

ApiRegistry::ApiRegistry() {
    // Initialize direct table to nullptr (atomically for thread-safety)
    for (auto &slot : m_DirectTable) {
        slot.store(nullptr, std::memory_order_relaxed);
    }
}

ApiRegistry &ApiRegistry::Instance() {
    static ApiRegistry registry;
    return registry;
}

void *ApiRegistry::Get(const std::string &name) const {
    std::shared_lock lock(g_Mutex);
    auto name_it = m_NameToId.find(name);
    if (name_it == m_NameToId.end())
        return nullptr;
    return ResolvePointerLocked(name_it->second, true);
}

void *ApiRegistry::GetWithoutCount(const std::string &name) const {
    std::shared_lock lock(g_Mutex);
    auto name_it = m_NameToId.find(name);
    if (name_it == m_NameToId.end())
        return nullptr;
    return ResolvePointerLocked(name_it->second, false);
}

uint64_t ApiRegistry::GetCallCount(const std::string &name) const {
    std::shared_lock lock(g_Mutex);
    auto name_it = m_NameToId.find(name);
    if (name_it == m_NameToId.end())
        return 0;
    auto entry_it = m_IdTable.find(name_it->second);
    return entry_it != m_IdTable.end() ? entry_it->second.call_count.load(std::memory_order_relaxed) : 0;
}

void ApiRegistry::Clear() {
    std::unique_lock lock(g_Mutex);
    m_IdTable.clear();
    m_NameToId.clear();
    m_Metadata.clear();
    m_StringStorage.clear();
    for (auto &slot : m_DirectTable) {
        slot.store(nullptr, std::memory_order_relaxed);
    }
    m_NextExtensionId.store(BML_EXTENSION_ID_START);
    m_TotalCapabilities.store(0);
    
    // Invalidate all TLS caches
    g_CacheVersion.fetch_add(1, std::memory_order_release);
}

// ========================================================================
// ID-Based Fast Path Implementation
// ========================================================================

void *ApiRegistry::GetById(BML_ApiId api_id) const {
    if (api_id == BML_API_INVALID_ID)
        return nullptr;
    
    std::shared_lock lock(g_Mutex);
    return ResolvePointerLocked(api_id, true);
}

void *ApiRegistry::GetByIdWithoutCount(BML_ApiId api_id) const {
    if (api_id == BML_API_INVALID_ID)
        return nullptr;
    
    std::shared_lock lock(g_Mutex);
    return ResolvePointerLocked(api_id, false);
}

uint64_t ApiRegistry::GetCallCountById(BML_ApiId api_id) const {
    if (api_id == BML_API_INVALID_ID)
        return 0;
    
    std::shared_lock lock(g_Mutex);
    auto it = m_IdTable.find(api_id);
    return it != m_IdTable.end() ? it->second.call_count.load(std::memory_order_relaxed) : 0;
}

bool ApiRegistry::GetApiId(const std::string &name, BML_ApiId *out_id) const {
    if (!out_id)
        return false;
    
    std::shared_lock lock(g_Mutex);
    auto it = m_NameToId.find(name);
    if (it != m_NameToId.end()) {
        *out_id = it->second;
        return true;
    }
    return false;
}

void ApiRegistry::RegisterCoreApiSet(const CoreApiDescriptor *descriptors, size_t count) {
    if (!descriptors || count == 0)
        return;

    uint32_t satisfied = 0u;
    std::vector<bool> completed(count, false);
    size_t remaining = count;

    while (remaining > 0) {
        bool progressed = false;

        for (size_t i = 0; i < count; ++i) {
            if (completed[i])
                continue;

            const auto &descriptor = descriptors[i];
            if ((descriptor.depends_mask & satisfied) != descriptor.depends_mask)
                continue;

            descriptor.register_fn();
            satisfied |= descriptor.provides_mask;
            completed[i] = true;
            --remaining;
            progressed = true;
        }

        if (!progressed) {
            throw std::runtime_error("Core API descriptor dependency cycle detected");
        }
    }
}

// ========================================================================
// Extended API Registration (Unified Core + Extension)
// ========================================================================

void ApiRegistry::RegisterApi(const ApiMetadata& metadata) {
    std::unique_lock lock(g_Mutex);
    RegisterApiLocked(metadata);
}

BML_ApiId ApiRegistry::RegisterExtension(
    const std::string& name,
    uint32_t version_major,
    uint32_t version_minor,
    const void* api_table,
    size_t api_size,
    const std::string& provider_id
) {
    std::unique_lock lock(g_Mutex);

    if (!CanRegisterLocked(name, BML_API_INVALID_ID)) {
        return BML_API_INVALID_ID;
    }

    BML_ApiId new_id = m_NextExtensionId.load(std::memory_order_relaxed);
    if (new_id >= BML_MAX_API_ID) {
        DebugWarning("Extension ID space exhausted");
        return BML_API_INVALID_ID;
    }
    m_NextExtensionId.fetch_add(1, std::memory_order_relaxed);

    auto name_ptr = std::make_unique<std::string>(name);
    auto provider_ptr = std::make_unique<std::string>(provider_id);

    const char* name_cstr = name_ptr->c_str();
    const char* provider_cstr = provider_ptr->c_str();

    m_StringStorage.push_back(std::move(name_ptr));
    m_StringStorage.push_back(std::move(provider_ptr));

    ApiMetadata meta;
    meta.name = name_cstr;
    meta.id = new_id;
    meta.pointer = const_cast<void*>(api_table);
    meta.version_major = static_cast<uint16_t>(version_major);
    meta.version_minor = static_cast<uint16_t>(version_minor);
    meta.version_patch = 0;
    meta.capabilities = BML_CAP_EXTENSION_BASIC;
    meta.type = BML_API_TYPE_EXTENSION;
    meta.threading = BML_THREADING_FREE;
    meta.provider_mod = provider_cstr;
    meta.description = nullptr;
    meta.api_size = api_size;

    RegisterApiLocked(meta);
    return new_id;
}

// ========================================================================
// API Querying and Discovery
// ========================================================================

const ApiRegistry::ApiMetadata* ApiRegistry::QueryApi(BML_ApiId api_id) const {
    std::shared_lock lock(g_Mutex);
    auto it = m_Metadata.find(api_id);
    return (it != m_Metadata.end()) ? &it->second : nullptr;
}

const ApiRegistry::ApiMetadata* ApiRegistry::QueryApi(const std::string& name) const {
    std::shared_lock lock(g_Mutex);
    auto id_it = m_NameToId.find(name);
    if (id_it == m_NameToId.end()) {
        return nullptr;
    }
    auto meta_it = m_Metadata.find(id_it->second);
    return (meta_it != m_Metadata.end()) ? &meta_it->second : nullptr;
}

bool ApiRegistry::TryGetMetadata(BML_ApiId api_id, ApiMetadata& out_meta) const {
    std::shared_lock lock(g_Mutex);
    auto it = m_Metadata.find(api_id);
    if (it == m_Metadata.end()) {
        return false;
    }
    out_meta = it->second;
    return true;
}

bool ApiRegistry::TryGetMetadata(const std::string& name, ApiMetadata& out_meta) const {
    std::shared_lock lock(g_Mutex);
    auto id_it = m_NameToId.find(name);
    if (id_it == m_NameToId.end()) {
        return false;
    }
    auto meta_it = m_Metadata.find(id_it->second);
    if (meta_it == m_Metadata.end()) {
        return false;
    }
    out_meta = meta_it->second;
    return true;
}

bool ApiRegistry::GetDescriptor(BML_ApiId api_id, BML_ApiDescriptor* out_desc) const {
    if (!out_desc) return false;
    
    std::shared_lock lock(g_Mutex);
    auto it = m_Metadata.find(api_id);
    if (it == m_Metadata.end()) {
        return false;
    }
    
    const ApiMetadata& meta = it->second;
    out_desc->id = meta.id;
    out_desc->name = meta.name;
    out_desc->type = meta.type;
    out_desc->version_major = meta.version_major;
    out_desc->version_minor = meta.version_minor;
    out_desc->version_patch = meta.version_patch;
    out_desc->reserved = 0;
    out_desc->capabilities = meta.capabilities;
    out_desc->threading = meta.threading;
    out_desc->provider_mod = meta.provider_mod;
    out_desc->description = meta.description;
    out_desc->call_count = meta.call_count.load(std::memory_order_relaxed);
    
    return true;
}

void ApiRegistry::Enumerate(
    BML_Bool (*callback)(BML_Context ctx, const BML_ApiDescriptor* desc, void* user_data),
    void* user_data,
    int type_filter
) const {
    if (!callback) return;

    std::shared_lock lock(g_Mutex);

    BML_Context ctx = Context::Instance().GetHandle();

    for (const auto& [id, meta] : m_Metadata) {
        // Apply type filter if specified
        if (type_filter >= 0 && static_cast<int>(meta.type) != type_filter) {
            continue;
        }

        BML_ApiDescriptor desc;
        desc.id = meta.id;
        desc.name = meta.name;
        desc.type = meta.type;
        desc.version_major = meta.version_major;
        desc.version_minor = meta.version_minor;
        desc.version_patch = meta.version_patch;
        desc.reserved = 0;
        desc.capabilities = meta.capabilities;
        desc.threading = meta.threading;
        desc.provider_mod = meta.provider_mod;
        desc.description = meta.description;
        desc.call_count = meta.call_count.load(std::memory_order_relaxed);

        if (!callback(ctx, &desc, user_data)) {
            break; // Callback requested stop
        }
    }
}

uint64_t ApiRegistry::GetTotalCapabilities() const {
    return m_TotalCapabilities.load(std::memory_order_relaxed);
}

size_t ApiRegistry::GetApiCount(int type_filter) const {
    std::shared_lock lock(g_Mutex);
    
    if (type_filter < 0) {
        return m_Metadata.size();
    }
    
    size_t count = 0;
    for (const auto& [id, meta] : m_Metadata) {
        if (static_cast<int>(meta.type) == type_filter) {
            ++count;
        }
    }
    return count;
}

// ========================================================================
// Performance: Direct Index Table and Caching
// ========================================================================

void* ApiRegistry::GetByIdDirect(BML_ApiId api_id) const {
    if (api_id == BML_API_INVALID_ID || api_id >= MAX_DIRECT_API_ID) {
        return nullptr;
    }
    
    // Direct array access guarded by shared lock for metadata consistency
    std::shared_lock lock(g_Mutex);
    
    void* ptr = m_DirectTable[api_id].load(std::memory_order_acquire);
    if (!ptr) {
        ptr = ResolvePointerLocked(api_id, false);
        if (ptr) {
            m_DirectTable[api_id].store(ptr, std::memory_order_release);
        }
    }

    if (!ptr)
        return nullptr;

    ResolvePointerLocked(api_id, true);
    return ptr;
}

void* ApiRegistry::GetByIdCached(BML_ApiId api_id) const {
    if (api_id == BML_API_INVALID_ID) {
        return nullptr;
    }

    while (true) {
        // Check if cache needs invalidation
        uint64_t current_version = g_CacheVersion.load(std::memory_order_acquire);
        if (g_TlsCacheVersion != current_version) {
            // Invalidate local cache
            for (size_t i = 0; i < TLS_CACHE_SIZE; ++i) {
                g_TlsCache[i].id = BML_API_INVALID_ID;
                g_TlsCache[i].ptr = nullptr;
            }
            g_TlsCacheNextSlot = 0;
            g_TlsCacheVersion = current_version;
        }
        
        for (size_t i = 0; i < TLS_CACHE_SIZE; ++i) {
            if (g_TlsCache[i].id != api_id || !g_TlsCache[i].ptr)
                continue;

            std::shared_lock lock(g_Mutex);
            if (g_CacheVersion.load(std::memory_order_acquire) != current_version) {
                break; // version changed, restart outer loop
            }

            void* ptr = ResolvePointerLocked(api_id, true);
            if (ptr && ptr == g_TlsCache[i].ptr) {
                return ptr;
            }

            g_TlsCache[i].id = BML_API_INVALID_ID;
            g_TlsCache[i].ptr = nullptr;
            break;
        }

        std::shared_lock lock(g_Mutex);
        if (g_CacheVersion.load(std::memory_order_acquire) != current_version) {
            continue;
        }

        void* ptr = nullptr;
        bool counted = false;
        if (api_id < MAX_DIRECT_API_ID) {
            ptr = m_DirectTable[api_id].load(std::memory_order_acquire);
        }
        if (!ptr) {
            ptr = ResolvePointerLocked(api_id, true);
            counted = ptr != nullptr;
            if (ptr && api_id < MAX_DIRECT_API_ID) {
                m_DirectTable[api_id].store(ptr, std::memory_order_release);
            }
        }

        if (!ptr) {
            return nullptr;
        }

        if (!counted) {
            ResolvePointerLocked(api_id, true);
        }

        g_TlsCache[g_TlsCacheNextSlot].id = api_id;
        g_TlsCache[g_TlsCacheNextSlot].ptr = ptr;
        g_TlsCacheNextSlot = (g_TlsCacheNextSlot + 1) % TLS_CACHE_SIZE;

        return ptr;
    }
}

// ========================================================================
// Extension Management (Unified with Core)
// ========================================================================

bool ApiRegistry::LoadVersioned(
    const std::string& name,
    uint32_t required_major,
    uint32_t required_minor,
    const void** out_api_table,
    uint32_t* out_actual_major,
    uint32_t* out_actual_minor
) const {
    if (!out_api_table) {
        return false;
    }
    
    std::shared_lock lock(g_Mutex);
    
    // Find by name
    auto id_it = m_NameToId.find(name);
    if (id_it == m_NameToId.end()) {
        return false;
    }
    
    auto meta_it = m_Metadata.find(id_it->second);
    if (meta_it == m_Metadata.end()) {
        return false;
    }
    
    const ApiMetadata& meta = meta_it->second;
    
    // Major version must match exactly (breaking changes)
    if (meta.version_major != required_major) {
        return false;
    }
    
    // Minor version must be >= required (backward-compatible additions)
    if (meta.version_minor < required_minor) {
        return false;
    }
    
    *out_api_table = meta.pointer;
    
    if (out_actual_major) {
        *out_actual_major = meta.version_major;
    }
    if (out_actual_minor) {
        *out_actual_minor = meta.version_minor;
    }
    
    return true;
}

size_t ApiRegistry::UnregisterByProvider(const std::string& provider_id) {
    std::unique_lock lock(g_Mutex);
    
    std::vector<BML_ApiId> ids_to_remove;
    
    // Find all APIs from this provider
    for (const auto& [id, meta] : m_Metadata) {
        if (meta.provider_mod && provider_id == meta.provider_mod) {
            ids_to_remove.push_back(id);
        }
    }
    
    // Remove them
    for (BML_ApiId id : ids_to_remove) {
        auto meta_it = m_Metadata.find(id);
        if (meta_it != m_Metadata.end()) {
            std::string name_str(meta_it->second.name ? meta_it->second.name : "");
            if (id < MAX_DIRECT_API_ID) {
                m_DirectTable[id].store(nullptr, std::memory_order_release);
            }
            m_Metadata.erase(id);
            m_IdTable.erase(id);
            if (!name_str.empty()) {
                m_NameToId.erase(name_str);
            }
        }
    }
    
    char buf[128];
    snprintf(buf, sizeof(buf), "Unregistered %zu APIs from provider: %s", 
             ids_to_remove.size(), provider_id.c_str());
    DebugInfo(buf);
    
    if (!ids_to_remove.empty()) {
        RecalculateTotalCapabilitiesLocked();
        g_CacheVersion.fetch_add(1, std::memory_order_release);
    }
    
    return ids_to_remove.size();
}

bool ApiRegistry::Unregister(const std::string& name) {
    std::unique_lock lock(g_Mutex);
    
    // Find by name
    auto id_it = m_NameToId.find(name);
    if (id_it == m_NameToId.end()) {
        return false;
    }
    
    BML_ApiId id = id_it->second;
    
    // Clear from direct table
    if (id < MAX_DIRECT_API_ID) {
        m_DirectTable[id].store(nullptr, std::memory_order_release);
    }
    
    // Remove from all maps
    m_Metadata.erase(id);
    m_IdTable.erase(id);
    m_NameToId.erase(name);
    RecalculateTotalCapabilitiesLocked();
    
    char buf[128];
    snprintf(buf, sizeof(buf), "Unregistered API: %s (ID=%u)", name.c_str(), id);
    DebugInfo(buf);
    
    // Invalidate TLS caches
    g_CacheVersion.fetch_add(1, std::memory_order_release);
    
    return true;
}

size_t ApiRegistry::GetExtensionCount() const {
    return GetApiCount(static_cast<int>(BML_API_TYPE_EXTENSION));
}

void ApiRegistry::RegisterApiLocked(const ApiMetadata& metadata) {
    if (metadata.id == BML_API_INVALID_ID) {
        DebugWarning("Attempted to register API with invalid ID (0)");
        return;
    }
    if (!metadata.name) {
        DebugWarning("Attempted to register API with null name");
        return;
    }

    std::string name_str(metadata.name);
    if (!CanRegisterLocked(name_str, metadata.id)) {
        return;
    }

    m_Metadata[metadata.id] = metadata;
    RegisterEntryLocked(name_str, metadata.pointer, metadata.id);
    m_TotalCapabilities.fetch_or(metadata.capabilities, std::memory_order_relaxed);

    char buf[256];
    snprintf(buf,
             sizeof(buf),
             "Registered API: %s (ID=%u, type=%d, caps=0x%llx)",
             metadata.name,
             metadata.id,
             metadata.type,
             static_cast<unsigned long long>(metadata.capabilities));
    DebugInfo(buf);
}

void ApiRegistry::RegisterEntryLocked(const std::string &name, void *pointer, BML_ApiId api_id) {
    ApiEntry entry;
    entry.pointer = pointer;
    m_IdTable.emplace(api_id, entry);
    m_NameToId.emplace(name, api_id);

    if (api_id < MAX_DIRECT_API_ID) {
        m_DirectTable[api_id].store(pointer, std::memory_order_release);
    }

    InvalidateTlsCachesLocked();
}

bool ApiRegistry::CanRegisterLocked(const std::string &name, BML_ApiId api_id) const {
    auto name_it = m_NameToId.find(name);
    if (name_it != m_NameToId.end()) {
        std::string warning = "Duplicate API registration for '" + name + "'";
        DebugWarning(warning.c_str());
        return false;
    }
    if (api_id != BML_API_INVALID_ID) {
        if (m_IdTable.find(api_id) != m_IdTable.end() || m_Metadata.find(api_id) != m_Metadata.end()) {
            char buf[128];
            snprintf(buf, sizeof(buf), "CRITICAL: API ID %u already registered", api_id);
            DebugWarning(buf);
            return false;
        }
    }
    return true;
}

void ApiRegistry::InvalidateTlsCachesLocked() {
    g_CacheVersion.fetch_add(1, std::memory_order_release);
}

void ApiRegistry::RecalculateTotalCapabilitiesLocked() {
    uint64_t total = 0;
    for (const auto &pair : m_Metadata) {
        total |= pair.second.capabilities;
    }
    m_TotalCapabilities.store(total, std::memory_order_relaxed);
}

void* ApiRegistry::ResolvePointerLocked(BML_ApiId api_id, bool increment_counts) const {
    auto it = m_IdTable.find(api_id);
    if (it == m_IdTable.end())
        return nullptr;
    void* ptr = it->second.pointer;
    if (ptr && increment_counts) {
        it->second.call_count.fetch_add(1, std::memory_order_relaxed);
        auto meta_it = m_Metadata.find(api_id);
        if (meta_it != m_Metadata.end()) {
            meta_it->second.call_count.fetch_add(1, std::memory_order_relaxed);
        }
    }
    return ptr;
}

} // namespace BML::Core
