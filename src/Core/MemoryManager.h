#ifndef BML_CORE_MEMORY_MANAGER_H
#define BML_CORE_MEMORY_MANAGER_H

#include <cstddef>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <string>

#include "bml_memory.h"
#include "FixedBlockPool.h"

namespace BML::Core {

    struct ModuleRange {
        uintptr_t base;
        uintptr_t end;
        std::string module_id;
        int index;
    };

    struct PerModuleStats {
        std::string module_id;
        std::atomic<uint64_t> total_allocated{0};
        std::atomic<uint64_t> peak_allocated{0};
        std::atomic<uint64_t> alloc_count{0};
        std::atomic<uint64_t> free_count{0};
        std::atomic<uint64_t> active_alloc_count{0};

        explicit PerModuleStats(const std::string &id) : module_id(id) {}
    };

    /// Plain snapshot for test / enumeration use (no atomics).
    struct ModuleMemorySnapshot {
        std::string module_id;
        uint64_t total_allocated;
        uint64_t peak_allocated;
        uint64_t alloc_count;
        uint64_t free_count;
        uint64_t active_alloc_count;
    };

    /**
     * @brief Memory pool implementation wrapper
     */
    struct MemoryPoolImpl {
        std::unique_ptr<FixedBlockPool> pool;
        size_t block_size;
        std::atomic<uint64_t> alloc_count{0};
        std::atomic<uint64_t> free_count{0};
    };

    /**
     * @brief Global memory manager singleton
     *
     * Provides unified memory allocation across DLL boundaries.
     * Thread-safe and tracks statistics when enabled.
     */
    class MemoryManager {
    public:
        MemoryManager(const MemoryManager &) = delete;
        MemoryManager &operator=(const MemoryManager &) = delete;

        // Basic allocation
        // Note: Standard Free() cannot track deallocation size accurately.
        // Use FreeWithSize() for precise statistics when the size is known.
        void *Alloc(size_t size);
        void *Calloc(size_t count, size_t size);
        void *Realloc(void *ptr, size_t old_size, size_t new_size);
        void *ReallocUnknownSize(void *ptr, size_t new_size); // Legacy, inaccurate tracking
        void Free(void *ptr);
        void FreeWithSize(void *ptr, size_t size); // For accurate deallocation tracking

        // Aligned allocation
        void *AllocAligned(size_t size, size_t alignment);
        void FreeAligned(void *ptr);

        // Memory pools
        BML_Result CreatePool(size_t block_size, uint32_t initial_blocks, BML_MemoryPool *out_pool);
        void *PoolAlloc(BML_MemoryPool pool);
        void PoolFree(BML_MemoryPool pool, void *ptr);
        void DestroyPool(BML_MemoryPool pool);

        // Statistics
        BML_Result GetStats(BML_MemoryStats *out_stats);

        // Configuration
        void SetTrackingEnabled(bool enabled) { m_tracking_enabled.store(enabled, std::memory_order_relaxed); }
        bool IsTrackingEnabled() const { return m_tracking_enabled.load(std::memory_order_relaxed); }

        // Per-module tracking
        void SetModuleRanges(std::vector<ModuleRange> ranges);
        void AddModuleRange(uintptr_t base, uintptr_t end, const std::string &module_id);
        void RemoveModuleRange(uintptr_t base);
        void EnablePerModuleTracking(bool enable);
        bool IsPerModuleTrackingEnabled() const;
        int FindModuleIndex(void *caller) const;

        void *AllocTracked(size_t size, void *caller);
        void *CallocTracked(size_t count, size_t size, void *caller);
        void *ReallocTracked(void *ptr, size_t old_size, size_t new_size, void *caller);
        void FreeTracked(void *ptr, void *caller);
        void *AllocAlignedTracked(size_t size, size_t alignment, void *caller);
        void FreeAlignedTracked(void *ptr, void *caller);

        std::vector<ModuleMemorySnapshot> GetPerModuleStats() const;
        BML_Result EnumerateModuleMemory(BML_EnumerateModuleMemoryFn cb, void *ud);

#if defined(BML_TEST)
        void ResetStatsForTesting();
#endif

        MemoryManager();
        ~MemoryManager() = default;

    private:
        // Allocation tracking
        void TrackAllocation(size_t size);
        void TrackDeallocation(size_t size);
        void TrackReallocation(size_t old_size, size_t new_size);
        void FreeInternal(void *ptr, size_t override_size, bool override_valid);

        // Pool management
        bool IsValidPool(BML_MemoryPool pool) const;

        void *ReallocInternal(void *ptr, size_t new_size);

        // Statistics
        std::atomic<uint64_t> m_TotalAllocated{0};
        std::atomic<uint64_t> m_PeakAllocated{0};
        std::atomic<uint64_t> m_AllocCount{0};
        std::atomic<uint64_t> m_FreeCount{0};
        std::atomic<uint64_t> m_ActiveAllocCount{0};

        // Configuration
        std::atomic<bool> m_tracking_enabled{true};
        static constexpr size_t kDefaultAlignment = alignof(std::max_align_t);
        static constexpr size_t kMinPoolBlockSize = 8;
        static constexpr size_t kMaxPoolBlockSize = 1024 * 1024; // 1 MB

        // Pool registry
        std::mutex m_PoolMutex;
        std::vector<std::unique_ptr<MemoryPoolImpl>> m_Pools;

        // Per-module tracking
        std::atomic<bool> m_PerModuleEnabled{false};
        mutable std::mutex m_RangeMutex;
        std::vector<ModuleRange> m_ModuleRanges;           // sorted by base
        std::vector<std::unique_ptr<PerModuleStats>> m_PerModuleStats;
        std::unique_ptr<PerModuleStats> m_CoreStats;       // for unresolved callers

        struct AllocRecord { int module_index; size_t size; };
        std::mutex m_AllocMapMutex;
        std::unordered_map<void *, AllocRecord> m_AllocMap;

        int FindModuleIndexLocked(void *caller) const;
        int ResolveModuleIndex(void *caller);
        PerModuleStats *GetModuleStats(int index);
    };
} // namespace BML::Core

#endif // BML_CORE_MEMORY_MANAGER_H
