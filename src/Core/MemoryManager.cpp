#include "MemoryManager.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>

#ifdef _WIN32
#include <malloc.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

#include "CoreErrors.h"
#include "FixedBlockPool.h"

namespace BML::Core {
    namespace {
        // Check if size is power of 2
        bool IsPowerOfTwo(size_t value) {
            return value > 0 && (value & (value - 1)) == 0;
        }

        // Align size up to alignment
        size_t AlignUp(size_t size, size_t alignment) {
            return (size + alignment - 1) & ~(alignment - 1);
        }

        constexpr uint32_t kAllocationMagic = 0xB00DA11Cu;
        constexpr uint32_t kAllocationFlagAligned = 1u << 0;

        struct AllocationMetadata {
            uint32_t magic{0};
            uint32_t flags{0};
            size_t size{0};
            void *original_ptr{nullptr};
        };

        constexpr size_t kMetadataAlignment = alignof(std::max_align_t);
        constexpr size_t kMetadataSize = (sizeof(AllocationMetadata) + kMetadataAlignment - 1) & ~(kMetadataAlignment - 1);
        static_assert((kMetadataSize % kMetadataAlignment) == 0, "Allocation metadata must stay aligned");

        bool TryAddSizes(size_t lhs, size_t rhs, size_t &out) {
            if (lhs > std::numeric_limits<size_t>::max() - rhs) {
                return false;
            }
            out = lhs + rhs;
            return true;
        }

        bool TryMultiplySizes(size_t lhs, size_t rhs, size_t &out) {
            if (lhs == 0 || rhs == 0) {
                out = 0;
                return true;
            }
            if (lhs > std::numeric_limits<size_t>::max() / rhs) {
                return false;
            }
            out = lhs * rhs;
            return true;
        }

        AllocationMetadata *GetMetadata(void *user_ptr) {
            if (!user_ptr) {
                return nullptr;
            }

            auto *metadata = reinterpret_cast<AllocationMetadata *>(
                reinterpret_cast<std::byte *>(user_ptr) - kMetadataSize);
            if (metadata->magic != kAllocationMagic) {
                return nullptr;
            }
            return metadata;
        }
    } // anonymous namespace

    MemoryManager &MemoryManager::Instance() {
        static MemoryManager instance;
        return instance;
    }

    MemoryManager::MemoryManager() = default;

    void *MemoryManager::Alloc(size_t size) {
        if (size == 0) {
            return nullptr;
        }

        size_t total_size = 0;
        if (!TryAddSizes(size, kMetadataSize, total_size)) {
            return nullptr;
        }

        void *raw_ptr = std::malloc(total_size);
        if (!raw_ptr) {
            return nullptr;
        }

        auto *metadata = reinterpret_cast<AllocationMetadata *>(raw_ptr);
        metadata->magic = kAllocationMagic;
        metadata->flags = 0;
        metadata->size = size;
        metadata->original_ptr = raw_ptr;

        if (m_tracking_enabled.load(std::memory_order_relaxed)) {
            TrackAllocation(size);
        }

        return reinterpret_cast<std::byte *>(raw_ptr) + kMetadataSize;
    }

    void *MemoryManager::Calloc(size_t count, size_t size) {
        size_t payload_size = 0;
        if (!TryMultiplySizes(count, size, payload_size)) {
            return nullptr;
        }

        if (payload_size == 0) {
            return nullptr;
        }

        size_t total_size = 0;
        if (!TryAddSizes(payload_size, kMetadataSize, total_size)) {
            return nullptr;
        }

        void *raw_ptr = std::calloc(1, total_size);
        if (!raw_ptr) {
            return nullptr;
        }

        auto *metadata = reinterpret_cast<AllocationMetadata *>(raw_ptr);
        metadata->magic = kAllocationMagic;
        metadata->flags = 0;
        metadata->size = payload_size;
        metadata->original_ptr = raw_ptr;

        if (m_tracking_enabled.load(std::memory_order_relaxed)) {
            TrackAllocation(payload_size);
        }

        return reinterpret_cast<std::byte *>(raw_ptr) + kMetadataSize;
    }

    void *MemoryManager::Realloc(void *ptr, size_t /*old_size*/, size_t new_size) {
        if (ptr == nullptr) {
            return Alloc(new_size);
        }

        if (new_size == 0) {
            Free(ptr);
            return nullptr;
        }

        return ReallocInternal(ptr, new_size);
    }

    void *MemoryManager::ReallocUnknownSize(void *ptr, size_t new_size) {
        if (ptr == nullptr) {
            return Alloc(new_size);
        }

        if (new_size == 0) {
            Free(ptr);
            return nullptr;
        }

        return ReallocInternal(ptr, new_size);
    }

    void MemoryManager::Free(void *ptr) {
        if (ptr == nullptr) {
            return;
        }

        auto *metadata = GetMetadata(ptr);
        if (!metadata) {
            return;
        }

        if (m_tracking_enabled.load(std::memory_order_relaxed)) {
            TrackDeallocation(metadata->size);
        }

        std::free(metadata->original_ptr);
    }

    void MemoryManager::FreeWithSize(void *ptr, size_t /*size*/) {
        Free(ptr);
    }

    void *MemoryManager::AllocAligned(size_t size, size_t alignment) {
        if (size == 0) {
            return nullptr;
        }

        if (!IsPowerOfTwo(alignment)) {
            return nullptr;
        }

        alignment = std::max(alignment, kMetadataAlignment);

        size_t total_size = 0;
        if (!TryAddSizes(size, kMetadataSize, total_size)) {
            return nullptr;
        }
        if (!TryAddSizes(total_size, alignment, total_size)) {
            return nullptr;
        }

        void *raw_ptr = std::malloc(total_size);
        if (!raw_ptr) {
            return nullptr;
        }

        uintptr_t raw_addr = reinterpret_cast<uintptr_t>(raw_ptr);
        uintptr_t aligned_addr = AlignUp(raw_addr + kMetadataSize, alignment);
        auto *metadata = reinterpret_cast<AllocationMetadata *>(aligned_addr - kMetadataSize);
        metadata->magic = kAllocationMagic;
        metadata->flags = kAllocationFlagAligned;
        metadata->size = size;
        metadata->original_ptr = raw_ptr;

        if (m_tracking_enabled.load(std::memory_order_relaxed)) {
            TrackAllocation(size);
        }

        return reinterpret_cast<void *>(aligned_addr);
    }

    void MemoryManager::FreeAligned(void *ptr) {
        Free(ptr);
    }

    BML_Result MemoryManager::CreatePool(size_t block_size, uint32_t initial_blocks, BML_MemoryPool *out_pool) {
        if (!out_pool) {
            return SetLastErrorAndReturn(BML_RESULT_INVALID_ARGUMENT, "memory", "bmlMemoryPoolCreate",
                                         "out_pool is NULL", 0);
        }

        if (block_size < kMinPoolBlockSize || block_size > kMaxPoolBlockSize) {
            return SetLastErrorAndReturn(BML_RESULT_INVALID_ARGUMENT, "memory", "bmlMemoryPoolCreate",
                                         "block_size out of valid range", 0);
        }

        try {
            auto pool_impl = std::make_unique<MemoryPoolImpl>();
            pool_impl->block_size = block_size;
            pool_impl->pool = std::make_unique<FixedBlockPool>(
                block_size,
                std::max<size_t>(initial_blocks, 16),
                64 // thread cache size
            );

            std::lock_guard<std::mutex> lock(m_pool_mutex);
            m_pools.push_back(std::move(pool_impl));
            *out_pool = reinterpret_cast<BML_MemoryPool>(m_pools.back().get());

            return BML_RESULT_OK;
        } catch (const std::bad_alloc &) {
            return SetLastErrorAndReturn(BML_RESULT_OUT_OF_MEMORY, "memory", "bmlMemoryPoolCreate",
                                         "Failed to allocate pool", 0);
        } catch (...) {
            return SetLastErrorAndReturn(BML_RESULT_UNKNOWN_ERROR, "memory", "bmlMemoryPoolCreate",
                                         "Unknown error creating pool", 0);
        }
    }

    void *MemoryManager::PoolAlloc(BML_MemoryPool pool) {
        if (!pool) {
            return nullptr;
        }

        auto *pool_impl = reinterpret_cast<MemoryPoolImpl *>(pool);
        if (!IsValidPool(pool)) {
            return nullptr;
        }

        void *ptr = pool_impl->pool->Allocate();
        if (ptr) {
            pool_impl->alloc_count.fetch_add(1, std::memory_order_relaxed);
        }

        return ptr;
    }

    void MemoryManager::PoolFree(BML_MemoryPool pool, void *ptr) {
        if (!pool || !ptr) {
            return;
        }

        auto *pool_impl = reinterpret_cast<MemoryPoolImpl *>(pool);
        if (!IsValidPool(pool)) {
            return;
        }

        pool_impl->pool->Deallocate(ptr);
        pool_impl->free_count.fetch_add(1, std::memory_order_relaxed);
    }

    void MemoryManager::DestroyPool(BML_MemoryPool pool) {
        if (!pool) {
            return;
        }

        std::lock_guard<std::mutex> lock(m_pool_mutex);

        auto it = std::find_if(m_pools.begin(), m_pools.end(),
                               [pool](const std::unique_ptr<MemoryPoolImpl> &p) {
                                   return p.get() == reinterpret_cast<MemoryPoolImpl *>(pool);
                               }
        );

        if (it != m_pools.end()) {
            // Check for leaked blocks
            auto *pool_impl = it->get();
            uint64_t allocs = pool_impl->alloc_count.load(std::memory_order_relaxed);
            uint64_t frees = pool_impl->free_count.load(std::memory_order_relaxed);
            if (allocs > frees) {
                // Log warning about leaked blocks
                char buf[256];
                snprintf(buf, sizeof(buf),
                         "[BML Memory] WARNING: Pool destroyed with %llu leaked blocks (block_size=%zu)\n",
                         (unsigned long long) (allocs - frees), pool_impl->block_size);
                OutputDebugStringA(buf);
            }
            m_pools.erase(it);
        }
    }

    BML_Result MemoryManager::GetStats(BML_MemoryStats *out_stats) {
        if (!out_stats) {
            return SetLastErrorAndReturn(BML_RESULT_INVALID_ARGUMENT, "memory", "bmlGetMemoryStats",
                                         "out_stats is NULL", 0);
        }

        if (!m_tracking_enabled.load(std::memory_order_relaxed)) {
            return SetLastErrorAndReturn(BML_RESULT_UNSUPPORTED, "memory", "bmlGetMemoryStats",
                                         "Memory tracking is disabled", 0);
        }

        out_stats->total_allocated = m_total_allocated.load(std::memory_order_relaxed);
        out_stats->peak_allocated = m_peak_allocated.load(std::memory_order_relaxed);
        out_stats->total_alloc_count = m_alloc_count.load(std::memory_order_relaxed);
        out_stats->total_free_count = m_free_count.load(std::memory_order_relaxed);
        out_stats->active_alloc_count = m_active_alloc_count.load(std::memory_order_relaxed);

        return BML_RESULT_OK;
    }

    BML_Result MemoryManager::GetCaps(BML_MemoryCaps *out_caps) {
        if (!out_caps) {
            return SetLastErrorAndReturn(BML_RESULT_INVALID_ARGUMENT, "memory", "bmlGetMemoryCaps",
                                         "out_caps is NULL", 0);
        }

        out_caps->struct_size = sizeof(BML_MemoryCaps);
        out_caps->api_version = bmlGetApiVersion();

        out_caps->capability_flags =
            BML_MEMORY_CAP_BASIC_ALLOC |
            BML_MEMORY_CAP_ALIGNED_ALLOC |
            BML_MEMORY_CAP_MEMORY_POOLS;

        if (m_tracking_enabled.load(std::memory_order_relaxed)) {
            out_caps->capability_flags |= BML_MEMORY_CAP_STATISTICS;
        }

        out_caps->default_alignment = kDefaultAlignment;
        out_caps->min_pool_block_size = kMinPoolBlockSize;
        out_caps->max_pool_block_size = kMaxPoolBlockSize;
        out_caps->threading_model = BML_THREADING_FREE;

        return BML_RESULT_OK;
    }

    void *MemoryManager::ReallocInternal(void *ptr, size_t new_size) {
        auto *metadata = GetMetadata(ptr);
        if (!metadata) {
            return nullptr;
        }

        size_t previous_size = metadata->size;
        if (metadata->flags & kAllocationFlagAligned) {
            void *replacement = Alloc(new_size);
            if (!replacement) {
                return nullptr;
            }

            std::memcpy(replacement, ptr, std::min(previous_size, new_size));
            Free(ptr);
            return replacement;
        }

        size_t total_size = 0;
        if (!TryAddSizes(new_size, kMetadataSize, total_size)) {
            return nullptr;
        }

        void *new_raw = std::realloc(metadata->original_ptr, total_size);
        if (!new_raw) {
            return nullptr;
        }

        auto *new_metadata = reinterpret_cast<AllocationMetadata *>(new_raw);
        new_metadata->magic = kAllocationMagic;
        new_metadata->flags = 0;
        new_metadata->size = new_size;
        new_metadata->original_ptr = new_raw;

        if (m_tracking_enabled.load(std::memory_order_relaxed)) {
            TrackReallocation(previous_size, new_size);
        }

        return reinterpret_cast<std::byte *>(new_raw) + kMetadataSize;
    }

    void MemoryManager::TrackAllocation(size_t size) {
        m_alloc_count.fetch_add(1, std::memory_order_relaxed);
        m_active_alloc_count.fetch_add(1, std::memory_order_relaxed);

        uint64_t new_total = m_total_allocated.fetch_add(size, std::memory_order_relaxed) + size;

        // Update peak (racy but acceptable for statistics)
        uint64_t current_peak = m_peak_allocated.load(std::memory_order_relaxed);
        while (new_total > current_peak) {
            if (m_peak_allocated.compare_exchange_weak(current_peak, new_total,
                                                       std::memory_order_relaxed)) {
                break;
            }
        }
    }

    void MemoryManager::TrackDeallocation(size_t size) {
        m_free_count.fetch_add(1, std::memory_order_relaxed);

        // Use compare-exchange to safely decrement active count without underflow
        uint64_t current = m_active_alloc_count.load(std::memory_order_relaxed);
        while (current > 0) {
            if (m_active_alloc_count.compare_exchange_weak(current, current - 1,
                                                           std::memory_order_relaxed, std::memory_order_relaxed)) {
                break;
            }
        }

        if (size > 0) {
            // Use compare-exchange to safely subtract without underflow
            uint64_t total = m_total_allocated.load(std::memory_order_relaxed);
            while (total >= size) {
                if (m_total_allocated.compare_exchange_weak(total, total - size,
                                                            std::memory_order_relaxed, std::memory_order_relaxed)) {
                    break;
                }
            }
        }
    }

    void MemoryManager::TrackReallocation(size_t old_size, size_t new_size) {
        if (new_size > old_size) {
            TrackAllocation(new_size - old_size);
        } else if (old_size > new_size) {
            TrackDeallocation(old_size - new_size);
        }
    }

    bool MemoryManager::IsValidPool(BML_MemoryPool pool) const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex &>(m_pool_mutex));

        return std::any_of(m_pools.begin(), m_pools.end(),
                           [pool](const std::unique_ptr<MemoryPoolImpl> &p) {
                               return p.get() == reinterpret_cast<MemoryPoolImpl *>(pool);
                           }
        );
    }

#if defined(BML_TEST)
    void MemoryManager::ResetStatsForTesting() {
        m_total_allocated.store(0, std::memory_order_relaxed);
        m_peak_allocated.store(0, std::memory_order_relaxed);
        m_alloc_count.store(0, std::memory_order_relaxed);
        m_free_count.store(0, std::memory_order_relaxed);
        m_active_alloc_count.store(0, std::memory_order_relaxed);
    }
#endif
} // namespace BML::Core
