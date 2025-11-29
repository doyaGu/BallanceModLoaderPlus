#ifndef BML_CORE_MEMORY_MANAGER_H
#define BML_CORE_MEMORY_MANAGER_H

#include <cstddef>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include "bml_memory.h"

namespace BML::Core {
    // Forward declarations
    class FixedBlockPool;

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
        static MemoryManager &Instance();

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
        BML_Result GetCaps(BML_MemoryCaps *out_caps);

        // Configuration
        void SetTrackingEnabled(bool enabled) { m_tracking_enabled.store(enabled, std::memory_order_relaxed); }
        bool IsTrackingEnabled() const { return m_tracking_enabled.load(std::memory_order_relaxed); }

#if defined(BML_TEST)
        void ResetStatsForTesting();
#endif

    private:
        MemoryManager();
        ~MemoryManager() = default;

        // Allocation tracking
        void TrackAllocation(size_t size);
        void TrackDeallocation(size_t size);
        void TrackReallocation(size_t old_size, size_t new_size);

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
    };
} // namespace BML::Core

#endif // BML_CORE_MEMORY_MANAGER_H
