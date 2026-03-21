#include "ApiRegistry.h"

#include "KernelServices.h"

#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <vector>

#include "Logging.h"

namespace BML::Core {
    namespace {
        std::shared_mutex g_Mutex;

        // Version counter for cache invalidation
        // Incremented whenever APIs are unregistered or cleared
        std::atomic<uint64_t> g_CacheVersion{0};

        // Thread-local cache for hot API lookups
        thread_local ApiRegistry::CacheEntry g_TlsCache[TLS_CACHE_SIZE];
        thread_local size_t g_TlsCacheNextSlot = 0;
        thread_local uint64_t g_TlsCacheVersion = 0;
    }

    ApiRegistry::ApiRegistry() {
        for (auto &slot : m_DirectTable) {
            slot.store(nullptr, std::memory_order_relaxed);
        }
    }

    // ========================================================================
    // Name-Based Lookup
    // ========================================================================

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
        return entry_it != m_IdTable.end()
            ? entry_it->second.call_count.load(std::memory_order_relaxed) : 0;
    }

    // ========================================================================
    // ID-Based Fast Path
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
        return it != m_IdTable.end()
            ? it->second.call_count.load(std::memory_order_relaxed) : 0;
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

    // ========================================================================
    // Direct Index Table and TLS Cache
    // ========================================================================

    void *ApiRegistry::GetByIdDirect(BML_ApiId api_id) const {
        if (api_id == BML_API_INVALID_ID || api_id >= MAX_DIRECT_API_ID)
            return nullptr;

        std::shared_lock lock(g_Mutex);
        void *ptr = m_DirectTable[api_id].load(std::memory_order_acquire);
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

    void *ApiRegistry::GetByIdCached(BML_ApiId api_id) const {
        if (api_id == BML_API_INVALID_ID)
            return nullptr;

        constexpr int kMaxRetries = 8;
        for (int retry = 0; retry < kMaxRetries; ++retry) {
            uint64_t current_version = g_CacheVersion.load(std::memory_order_acquire);
            if (g_TlsCacheVersion != current_version) {
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
                if (g_CacheVersion.load(std::memory_order_acquire) != current_version)
                    break;

                void *ptr = ResolvePointerLocked(api_id, true);
                if (ptr && ptr == g_TlsCache[i].ptr)
                    return ptr;

                g_TlsCache[i].id = BML_API_INVALID_ID;
                g_TlsCache[i].ptr = nullptr;
                break;
            }

            std::shared_lock lock(g_Mutex);
            if (g_CacheVersion.load(std::memory_order_acquire) != current_version)
                continue;

            void *ptr = nullptr;
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
            if (!ptr)
                return nullptr;
            if (!counted)
                ResolvePointerLocked(api_id, true);

            g_TlsCache[g_TlsCacheNextSlot].id = api_id;
            g_TlsCache[g_TlsCacheNextSlot].ptr = ptr;
            g_TlsCacheNextSlot = (g_TlsCacheNextSlot + 1) % TLS_CACHE_SIZE;
            return ptr;
        }

        std::shared_lock lock(g_Mutex);
        return ResolvePointerLocked(api_id, true);
    }

    // ========================================================================
    // Registration
    // ========================================================================

    void ApiRegistry::Clear() {
        std::unique_lock lock(g_Mutex);
        m_IdTable.clear();
        m_NameToId.clear();
        m_Metadata.clear();
        m_StringStorage.clear();
        for (auto &slot : m_DirectTable) {
            slot.store(nullptr, std::memory_order_relaxed);
        }
        m_NextAutoId.store(1);
        g_CacheVersion.fetch_add(1, std::memory_order_release);
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

    void ApiRegistry::RegisterApi(const char *name, void *pointer, const char *provider_mod) {
        if (!name) {
            CoreLog(BML_LOG_WARN, "api.registry", "Attempted to register API with null name");
            return;
        }
        if (!pointer) {
            CoreLog(BML_LOG_WARN, "api.registry", "Attempted to register API '%s' with null pointer", name);
            return;
        }

        std::unique_lock lock(g_Mutex);

        // Auto-assign an ID
        BML_ApiId new_id = m_NextAutoId.fetch_add(1, std::memory_order_relaxed);

        ApiMetadata meta;
        meta.name = StoreString(name);
        meta.id = new_id;
        meta.pointer = pointer;
        meta.provider_mod = provider_mod ? StoreString(provider_mod) : nullptr;

        RegisterApiLocked(meta);
    }

    // ========================================================================
    // Unregistration
    // ========================================================================

    size_t ApiRegistry::UnregisterByProvider(const std::string &provider_id) {
        std::unique_lock lock(g_Mutex);

        std::vector<BML_ApiId> ids_to_remove;
        for (const auto &[id, meta] : m_Metadata) {
            if (meta.provider_mod && provider_id == meta.provider_mod) {
                ids_to_remove.push_back(id);
            }
        }

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

        if (!ids_to_remove.empty()) {
            CoreLog(BML_LOG_DEBUG, "api.registry", "Unregistered %zu APIs from provider: %s",
                    ids_to_remove.size(), provider_id.c_str());
            g_CacheVersion.fetch_add(1, std::memory_order_release);
        }

        return ids_to_remove.size();
    }

    bool ApiRegistry::Unregister(const std::string &name) {
        std::unique_lock lock(g_Mutex);

        auto id_it = m_NameToId.find(name);
        if (id_it == m_NameToId.end())
            return false;

        BML_ApiId id = id_it->second;
        if (id < MAX_DIRECT_API_ID) {
            m_DirectTable[id].store(nullptr, std::memory_order_release);
        }

        m_Metadata.erase(id);
        m_IdTable.erase(id);
        m_NameToId.erase(name);

        CoreLog(BML_LOG_DEBUG, "api.registry", "Unregistered API: %s (ID=%u)", name.c_str(), id);
        g_CacheVersion.fetch_add(1, std::memory_order_release);
        return true;
    }

    size_t ApiRegistry::GetApiCount() const {
        std::shared_lock lock(g_Mutex);
        return m_Metadata.size();
    }

    // ========================================================================
    // Internal Helpers
    // ========================================================================

    void ApiRegistry::RegisterApiLocked(const ApiMetadata &metadata) {
        if (metadata.id == BML_API_INVALID_ID) {
            CoreLog(BML_LOG_WARN, "api.registry", "Attempted to register API with invalid ID (0)");
            return;
        }
        if (!metadata.name) {
            CoreLog(BML_LOG_WARN, "api.registry", "Attempted to register API with null name");
            return;
        }
        if (!metadata.pointer) {
            CoreLog(BML_LOG_WARN,
                    "api.registry",
                    "Attempted to register API '%s' with null pointer",
                    metadata.name);
            return;
        }

        std::string name_str(metadata.name);
        if (!CanRegisterLocked(name_str, metadata.id))
            return;

        m_Metadata[metadata.id] = metadata;
        RegisterEntryLocked(name_str, metadata.pointer, metadata.id);

        CoreLog(BML_LOG_DEBUG, "api.registry", "Registered API: %s (ID=%u)", metadata.name, metadata.id);
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
            CoreLog(BML_LOG_WARN, "api.registry", "Duplicate API registration for '%s'", name.c_str());
            return false;
        }
        if (api_id != BML_API_INVALID_ID) {
            if (m_IdTable.find(api_id) != m_IdTable.end() || m_Metadata.find(api_id) != m_Metadata.end()) {
                CoreLog(BML_LOG_WARN, "api.registry", "CRITICAL: API ID %u already registered", api_id);
                return false;
            }
        }
        return true;
    }

    void ApiRegistry::InvalidateTlsCachesLocked() {
        g_CacheVersion.fetch_add(1, std::memory_order_release);
    }

    const char *ApiRegistry::StoreString(const char *value) {
        if (!value)
            return nullptr;
        auto stored = std::make_unique<std::string>(value);
        const char *ptr = stored->c_str();
        m_StringStorage.push_back(std::move(stored));
        return ptr;
    }

    void *ApiRegistry::ResolvePointerLocked(BML_ApiId api_id, bool increment_counts) const {
        auto it = m_IdTable.find(api_id);
        if (it == m_IdTable.end())
            return nullptr;
        void *ptr = it->second.pointer;
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
