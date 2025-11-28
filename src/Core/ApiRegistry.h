#ifndef BML_CORE_API_REGISTRY_H
#define BML_CORE_API_REGISTRY_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "bml_capabilities.h"
#include "bml_types.h"

namespace BML::Core {
    /**
     * @brief 32-bit API identifier for fast lookup
     *
     * CRITICAL: IDs are explicitly assigned and PERMANENT (like syscall numbers).
     * Once assigned, an ID never changes across BML versions, ensuring binary compatibility.
     *
     * Using integer IDs instead of string lookups provides:
     * - ~3-5x faster lookup (direct integer map access)
     * - Better cache locality (4 bytes vs variable-length strings)
     * - Zero allocation overhead
     * - Guaranteed stability across versions
     *
     * IDs are defined in bml_api_ids.h and must be used for registration.
     */
    typedef uint32_t BML_ApiId;

    /** @brief Invalid/unregistered API ID sentinel */
    static constexpr BML_ApiId BML_API_INVALID_ID = 0u;

    /** @brief Maximum API ID for direct indexing (performance optimization) */
    static constexpr size_t MAX_DIRECT_API_ID = 10000;

    /** @brief Thread-local cache size for hot API lookups */
    static constexpr size_t TLS_CACHE_SIZE = 16;

    class ApiRegistry {
    public:
        /**
         * @brief Legacy API entry structure (maintained for compatibility)
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
         * @brief Extended API metadata with full feature support
         *
         * This structure unifies core API and extension API metadata,
         * providing version info, capabilities, and threading model.
         */
        struct ApiMetadata {
            const char *name{nullptr}; /**< API name (e.g., "bmlImcPublish") */
            BML_ApiId id{0};           /**< Stable API ID */
            void *pointer{nullptr};    /**< Function/table pointer */

            /* Version information */
            uint16_t version_major{0}; /**< Version when introduced */
            uint16_t version_minor{0};
            uint16_t version_patch{0};

            /* Capability and classification */
            uint64_t capabilities{0};                         /**< Capability flags provided */
            BML_ApiType type{BML_API_TYPE_CORE};              /**< API type classification */
            BML_ThreadingModel threading{BML_THREADING_FREE}; /**< Thread safety */

            /* Provider information */
            const char *provider_mod{nullptr}; /**< Provider mod ("BML" for core) */
            const char *description{nullptr};  /**< Human-readable description */

            /* API table info (for extensions) */
            size_t api_size{0}; /**< Size of API table (extensions) */

            /* Runtime statistics */
            mutable std::atomic<uint64_t> call_count{0};

            ApiMetadata() = default;

            ApiMetadata(const ApiMetadata &other)
                : name(other.name),
                  id(other.id),
                  pointer(other.pointer),
                  version_major(other.version_major),
                  version_minor(other.version_minor),
                  version_patch(other.version_patch),
                  capabilities(other.capabilities),
                  type(other.type),
                  threading(other.threading),
                  provider_mod(other.provider_mod),
                  description(other.description),
                  api_size(other.api_size),
                  call_count(other.call_count.load()) {
            }

            ApiMetadata &operator=(const ApiMetadata &other) {
                if (this != &other) {
                    name = other.name;
                    id = other.id;
                    pointer = other.pointer;
                    version_major = other.version_major;
                    version_minor = other.version_minor;
                    version_patch = other.version_patch;
                    capabilities = other.capabilities;
                    type = other.type;
                    threading = other.threading;
                    provider_mod = other.provider_mod;
                    description = other.description;
                    api_size = other.api_size;
                    call_count.store(other.call_count.load());
                }
                return *this;
            }
        };

        /**
         * @brief Thread-local cache entry for hot API lookups
         */
        struct CacheEntry {
            BML_ApiId id{BML_API_INVALID_ID};
            void *ptr{nullptr};
        };

        static ApiRegistry &Instance();

        struct CoreApiDescriptor {
            const char *name;
            void (*register_fn)();
            uint32_t provides_mask;
            uint32_t depends_mask;
        };

        void *Get(const std::string &name) const;
        void *GetWithoutCount(const std::string &name) const;
        uint64_t GetCallCount(const std::string &name) const;

        // ID-based fast path API
        /**
         * @brief Get API pointer using pre-computed ID (fast path)
         *
         * Performance: ~3-5x faster than string-based Get()
         * - Bypasses string hashing and comparison
         * - Direct integer-based map lookup
         * - Ideal for high-frequency API calls
         *
         * @param api_id Pre-computed API ID from BML_API_ID() or RegisterApiId()
         * @return API function pointer, or nullptr if not found
         */
        void *GetById(BML_ApiId api_id) const;
        void *GetByIdWithoutCount(BML_ApiId api_id) const;
        uint64_t GetCallCountById(BML_ApiId api_id) const;

        /**
         * @brief Get API ID for registered API name
         *
         * Returns the 32-bit ID for an API name. The ID remains constant
         * for the process lifetime and matches compile-time BML_API_ID().
         *
         * @param name API function name (e.g., "bmlImcPublish")
         * @param out_id Receives the API ID if found
         * @return true if API is registered, false otherwise
         */
        bool GetApiId(const std::string &name, BML_ApiId *out_id) const;

        void Clear();
        void RegisterCoreApiSet(const CoreApiDescriptor *descriptors, size_t count);

        /**
         * @brief Register an API with full metadata
         *
         * Registers an API using the provided metadata structure.
         *
         * @param metadata Full API metadata structure
         */
        void RegisterApi(const ApiMetadata &metadata);

        /**
         * @brief Register an extension API and auto-assign ID
         *
         * @param name Extension name
         * @param version_major Major version
         * @param version_minor Minor version
         * @param api_table Pointer to API table
         * @param api_size Size of API table
         * @param provider_id Provider mod ID
         * @return Assigned API ID (50000+), or 0 on failure
         */
        BML_ApiId RegisterExtension(
            const std::string &name,
            uint32_t version_major,
            uint32_t version_minor,
            const void *api_table,
            size_t api_size,
            const std::string &provider_id
        );

        /**
         * @brief Load an extension API with version checking
         *
         * @param name Extension name
         * @param required_major Required major version (must match exactly)
         * @param required_minor Minimum required minor version
         * @param out_api_table Receives API table pointer
         * @param out_actual_major Receives actual major version (optional)
         * @param out_actual_minor Receives actual minor version (optional)
         * @return true if compatible version found and loaded
         */
        bool LoadVersioned(
            const std::string &name,
            uint32_t required_major,
            uint32_t required_minor,
            const void **out_api_table,
            uint32_t *out_actual_major = nullptr,
            uint32_t *out_actual_minor = nullptr
        ) const;

        /**
         * @brief Unregister all APIs provided by a specific mod
         *
         * @param provider_id Mod ID to remove APIs for
         * @return Number of APIs removed
         */
        size_t UnregisterByProvider(const std::string &provider_id);

        /**
         * @brief Unregister a specific API by name
         *
         * @param name API name to unregister
         * @return true if API was found and removed
         */
        bool Unregister(const std::string &name);

        /**
         * @brief Get extension count
         *
         * @return Number of registered extension APIs
         */
        size_t GetExtensionCount() const;

        /* ====================================================================
         * API Querying and Discovery
         * ==================================================================== */

        /**
         * @brief Get full metadata for an API by ID
         *
         * @param api_id API ID to query
         * @return Pointer to metadata, or nullptr if not found
         */
        const ApiMetadata *QueryApi(BML_ApiId api_id) const;

        /**
         * @brief Get full metadata for an API by name
         *
         * @param name API name to query
         * @return Pointer to metadata, or nullptr if not found
         */
        const ApiMetadata *QueryApi(const std::string &name) const;

        /**
         * @brief Copy metadata for an API by ID in a thread-safe manner
         */
        bool TryGetMetadata(BML_ApiId api_id, ApiMetadata &out_meta) const;

        /**
         * @brief Copy metadata for an API by name in a thread-safe manner
         */
        bool TryGetMetadata(const std::string &name, ApiMetadata &out_meta) const;

        /**
         * @brief Fill a BML_ApiDescriptor from internal metadata
         *
         * @param api_id API ID to query
         * @param out_desc Pointer to receive descriptor
         * @return true if found, false otherwise
         */
        bool GetDescriptor(BML_ApiId api_id, BML_ApiDescriptor *out_desc) const;

        /**
         * @brief Enumerate all registered APIs
         *
         * @param callback Function called for each API
         * @param user_data User context
         * @param type_filter Filter by type (or -1 for all)
         */
        void Enumerate(
            BML_Bool (*callback)(BML_Context ctx, const BML_ApiDescriptor *desc, void *user_data),
            void *user_data,
            int type_filter
        ) const;

        /**
         * @brief Get combined capabilities of all registered APIs
         *
         * @return Bitmask of all available capabilities
         */
        uint64_t GetTotalCapabilities() const;

        /**
         * @brief Get count of registered APIs
         *
         * @param type_filter Filter by type (or -1 for all)
         * @return Number of registered APIs
         */
        size_t GetApiCount(int type_filter = -1) const;

        /* ====================================================================
         * Performance: Direct Index Table
         * ==================================================================== */

        /**
         * @brief Get API pointer using direct index (ultra-fast path)
         *
         * Performance: O(1) array access, ~5x faster than hash lookup
         * Only works for IDs < MAX_DIRECT_API_ID
         *
         * @param api_id API ID (must be < MAX_DIRECT_API_ID)
         * @return API pointer, or nullptr if not found
         */
        void *GetByIdDirect(BML_ApiId api_id) const;

        /**
         * @brief Get API pointer with thread-local caching
         *
         * Uses a small thread-local cache for frequently accessed APIs.
         * Falls back to GetById on cache miss.
         *
         * @param api_id API ID to lookup
         * @return API pointer, or nullptr if not found
         */
        void *GetByIdCached(BML_ApiId api_id) const;

    private:
        ApiRegistry();
        ~ApiRegistry() = default;

        // Dual indexing: string map (legacy) + ID map (fast path)
        std::unordered_map<BML_ApiId, ApiEntry> m_IdTable;     // Fast lookup by ID
        std::unordered_map<std::string, BML_ApiId> m_NameToId; // Name -> ID mapping

        // Extended metadata storage
        std::unordered_map<BML_ApiId, ApiMetadata> m_Metadata;

        // String storage for dynamically registered APIs (ensures lifetime)
        std::vector<std::unique_ptr<std::string>> m_StringStorage;

        // Direct index table for ultra-fast lookup (IDs < MAX_DIRECT_API_ID)
        mutable std::atomic<void *> m_DirectTable[MAX_DIRECT_API_ID] = {};

        // Extension ID allocation
        std::atomic<BML_ApiId> m_NextExtensionId{BML_EXTENSION_ID_START};

        // Aggregate capabilities
        std::atomic<uint64_t> m_TotalCapabilities{0};

        void RegisterApiLocked(const ApiMetadata &metadata);
        void RegisterEntryLocked(const std::string &name, void *pointer, BML_ApiId api_id);
        bool CanRegisterLocked(const std::string &name, BML_ApiId api_id) const;
        void InvalidateTlsCachesLocked();
        void RecalculateTotalCapabilitiesLocked();
        void *ResolvePointerLocked(BML_ApiId api_id, bool increment_counts) const;
    };
} // namespace BML::Core

#endif // BML_CORE_API_REGISTRY_H
