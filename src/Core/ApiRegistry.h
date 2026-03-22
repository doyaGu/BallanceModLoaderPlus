#ifndef BML_CORE_API_REGISTRY_H
#define BML_CORE_API_REGISTRY_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "bml_types.h"

namespace BML::Core {
    /**
     * @brief 32-bit API identifier for fast lookup
     *
     * IDs are auto-assigned at registration time and remain stable
     * for the process lifetime.
     */
    typedef uint32_t BML_ApiId;

    /** @brief Invalid/unregistered API ID sentinel */
    static constexpr BML_ApiId BML_API_INVALID_ID = 0u;

    /** @brief Maximum API ID for direct indexing (performance optimization) */
    static constexpr size_t MAX_DIRECT_API_ID = 10000;

    class ApiRegistry {
    public:
        /**
         * @brief API entry for fast-path lookups
         */
        struct ApiEntry {
            void *pointer{nullptr};
            mutable std::atomic<uint64_t> call_count{0};

            ApiEntry() = default;

            ApiEntry(const ApiEntry &other) {
                pointer = other.pointer;
                call_count.store(other.call_count.load(std::memory_order_relaxed), std::memory_order_relaxed);
            }

            ApiEntry &operator=(const ApiEntry &other) {
                if (this != &other) {
                    pointer = other.pointer;
                    call_count.store(other.call_count.load(std::memory_order_relaxed), std::memory_order_relaxed);
                }
                return *this;
            }

            ApiEntry(ApiEntry &&other) noexcept {
                pointer = other.pointer;
                call_count.store(other.call_count.load(std::memory_order_relaxed), std::memory_order_relaxed);
            }

            ApiEntry &operator=(ApiEntry &&other) noexcept {
                if (this != &other) {
                    pointer = other.pointer;
                    call_count.store(other.call_count.load(std::memory_order_relaxed), std::memory_order_relaxed);
                }
                return *this;
            }
        };

        /**
         * @brief API metadata stored per registration
         */
        struct ApiMetadata {
            const char *name{nullptr};         /**< API name (e.g., "bmlImcPublish") */
            BML_ApiId id{0};                   /**< Auto-assigned stable ID */
            void *pointer{nullptr};            /**< Function/table pointer */
            const char *provider_mod{nullptr};  /**< Provider mod ("BML" for core) */
            mutable std::atomic<uint64_t> call_count{0};

            ApiMetadata() = default;

            ApiMetadata(const ApiMetadata &other)
                : name(other.name),
                  id(other.id),
                  pointer(other.pointer),
                  provider_mod(other.provider_mod),
                  call_count(other.call_count.load()) {
            }

            ApiMetadata &operator=(const ApiMetadata &other) {
                if (this != &other) {
                    name = other.name;
                    id = other.id;
                    pointer = other.pointer;
                    provider_mod = other.provider_mod;
                    call_count.store(other.call_count.load());
                }
                return *this;
            }
        };

        struct CoreApiDescriptor {
            const char *name;
            void (*register_fn)();
            uint32_t provides_mask;
            uint32_t depends_mask;
        };

        // Name-based lookup (primary path used by bmlGetProcAddress)
        void *Get(const std::string &name) const;
        void *GetWithoutCount(const std::string &name) const;
        uint64_t GetCallCount(const std::string &name) const;

        // ID-based fast path
        void *GetById(BML_ApiId api_id) const;
        void *GetByIdWithoutCount(BML_ApiId api_id) const;
        uint64_t GetCallCountById(BML_ApiId api_id) const;
        bool GetApiId(const std::string &name, BML_ApiId *out_id) const;

        // Direct index table (ultra-fast O(1) for IDs < MAX_DIRECT_API_ID)
        void *GetByIdDirect(BML_ApiId api_id) const;
        void *GetByIdCached(BML_ApiId api_id) const;

        void Clear();
        void RegisterCoreApiSet(const CoreApiDescriptor *descriptors, size_t count);

        /**
         * @brief Register an API by name
         *
         * Auto-assigns an internal ID for fast-path lookups.
         */
        void RegisterApi(const char *name, void *pointer, const char *provider_mod = "BML");

        size_t UnregisterByProvider(const std::string &provider_id);
        bool Unregister(const std::string &name);

        size_t GetApiCount() const;

        ApiRegistry();
        ~ApiRegistry() = default;

    private:

        // Dual indexing: ID table (fast path) + name->ID mapping
        std::unordered_map<BML_ApiId, ApiEntry> m_IdTable;
        std::unordered_map<std::string, BML_ApiId> m_NameToId;

        // Metadata storage
        std::unordered_map<BML_ApiId, ApiMetadata> m_Metadata;

        // String storage (ensures lifetime of dynamically registered names)
        std::vector<std::unique_ptr<std::string>> m_StringStorage;

        // Direct index table for ultra-fast lookup (IDs < MAX_DIRECT_API_ID)
        mutable std::atomic<void *> m_DirectTable[MAX_DIRECT_API_ID] = {};

        // Auto-assigned ID counter
        std::atomic<BML_ApiId> m_NextAutoId{1};

        void RegisterApiLocked(const ApiMetadata &metadata);
        void RegisterEntryLocked(const std::string &name, void *pointer, BML_ApiId api_id);
        bool CanRegisterLocked(const std::string &name, BML_ApiId api_id) const;
        void IncrementCallCountLocked(BML_ApiId api_id) const;
        const char *StoreString(const char *value);
        void *ResolvePointerLocked(BML_ApiId api_id, bool increment_counts) const;
    };
} // namespace BML::Core

#endif // BML_CORE_API_REGISTRY_H
