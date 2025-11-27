#ifndef BML_CORE_FIXED_BLOCK_POOL_H
#define BML_CORE_FIXED_BLOCK_POOL_H

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <new>
#include <utility>
#include <vector>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

namespace BML::Core {

// Lock-free fixed-size block allocator with thread-local caches.
// Blocks are carved from chunks and recycled without touching the OS once warmed up.
class FixedBlockPool {
public:
        FixedBlockPool(size_t blockSize,
                                     size_t blocksPerChunk = 1024,
                                     size_t threadCacheSize = 64)
        : m_BlockSize(blockSize),
          m_BlockStride(AlignUp(std::max(blockSize, sizeof(FreeNode)), alignof(std::max_align_t))),
          m_BlocksPerChunk(std::max<size_t>(blocksPerChunk, 1)),
          m_MaxCacheSize(std::max<size_t>(threadCacheSize, 8)) {
        AllocateChunk();
    }

    ~FixedBlockPool() = default;

    FixedBlockPool(const FixedBlockPool &) = delete;
    FixedBlockPool &operator=(const FixedBlockPool &) = delete;

    void *Allocate() {
        auto &cache = GetThreadCache();
        while (!cache.head) {
            if (RefillCache(cache))
                break;
            AllocateChunk();
        }
        auto *node = cache.head;
        if (!node)
            return nullptr;
        cache.head = node->next;
        cache.size -= 1;
        return static_cast<void *>(node);
    }

    template <typename T, typename... Args>
    T *Construct(Args &&...args) {
        if (sizeof(T) > m_BlockSize)
            return nullptr;
        void *memory = Allocate();
        if (!memory)
            return nullptr;
        return new (memory) T(std::forward<Args>(args)...);
    }

    template <typename T>
    void Destroy(T *object) {
        if (!object)
            return;
        object->~T();
        Deallocate(object);
    }

    void Deallocate(void *ptr) {
        if (!ptr)
            return;
        auto &cache = GetThreadCache();
        auto *node = static_cast<FreeNode *>(ptr);
        node->next = cache.head;
        cache.head = node;
        cache.size += 1;
        if (cache.size >= m_MaxCacheSize) {
            FlushCache(cache);
        }
    }

    size_t BlockSize() const { return m_BlockSize; }
    size_t BlockStride() const { return m_BlockStride; }

private:
    struct FreeNode {
        FreeNode *next{nullptr};
    };

    struct ThreadCache {
        FreeNode *head{nullptr};
        size_t size{0};
    };

    struct ListSegment {
        FreeNode *head{nullptr};
        FreeNode *tail{nullptr};
        size_t count{0};
    };

    static size_t AlignUp(size_t value, size_t alignment) {
        const size_t mask = alignment - 1;
        return (value + mask) & ~mask;
    }

    ThreadCache &GetThreadCache() {
        struct CacheEntry {
            FixedBlockPool *owner{nullptr};
            ThreadCache cache{};
        };
        static thread_local std::vector<CacheEntry> caches;
        for (auto &entry : caches) {
            if (entry.owner == this)
                return entry.cache;
        }
        caches.push_back(CacheEntry{this, {}});
        return caches.back().cache;
    }

    bool RefillCache(ThreadCache &cache) {
        const size_t target = std::max<size_t>(m_MaxCacheSize / 2, 8);
        auto segment = AcquireFromGlobal(target);
        if (!segment.head)
            return false;
        segment.tail->next = cache.head;
        cache.head = segment.head;
        cache.size += segment.count;
        return true;
    }

    void FlushCache(ThreadCache &cache) {
        const size_t keep = m_MaxCacheSize / 2;
        if (cache.size <= keep)
            return;
        const size_t release = cache.size - keep;
        FreeNode *tail = cache.head;
        FreeNode *prev = nullptr;
        for (size_t i = 0; i < release; ++i) {
            prev = tail;
            tail = tail->next;
        }
        if (!prev)
            return;
        FreeNode *releaseHead = cache.head;
        FreeNode *releaseTail = prev;
        cache.head = tail;
        cache.size -= release;
        releaseTail->next = nullptr;
        PushToGlobal(releaseHead, releaseTail);
    }

    void AllocateChunk() {
        auto buffer = std::make_unique<std::byte[]>(m_BlockStride * m_BlocksPerChunk);
        FreeNode *head = nullptr;
        FreeNode *tail = nullptr;
        for (size_t i = 0; i < m_BlocksPerChunk; ++i) {
            auto *node = reinterpret_cast<FreeNode *>(buffer.get() + i * m_BlockStride);
            new (node) FreeNode{};
            node->next = head;
            head = node;
            if (!tail)
                tail = node;
        }
        if (!head)
            return;
        {
            std::scoped_lock lock(m_ChunkMutex);
            m_Chunks.push_back(std::move(buffer));
        }
        PushToGlobal(head, tail);
    }

    ListSegment AcquireFromGlobal(size_t desired) {
        ListSegment result;
        if (desired == 0)
            return result;
        desired = std::min(desired, m_BlocksPerChunk);
        while (true) {
            FreeNode *head = m_GlobalFreeList.load(std::memory_order_acquire);
            if (!head)
                return result;
            FreeNode *tail = head;
            size_t count = 1;
            while (count < desired && tail->next) {
                tail = tail->next;
                ++count;
            }
            FreeNode *next = tail->next;
            if (m_GlobalFreeList.compare_exchange_weak(head, next, std::memory_order_acq_rel, std::memory_order_acquire)) {
                tail->next = nullptr;
                result.head = head;
                result.tail = tail;
                result.count = count;
                return result;
            }
        }
    }

    void PushToGlobal(FreeNode *head, FreeNode *tail) {
        if (!head || !tail)
            return;
        while (true) {
            FreeNode *oldHead = m_GlobalFreeList.load(std::memory_order_relaxed);
            tail->next = oldHead;
            if (m_GlobalFreeList.compare_exchange_weak(oldHead, head, std::memory_order_release, std::memory_order_relaxed)) {
                return;
            }
        }
    }

    const size_t m_BlockSize;
    const size_t m_BlockStride;
    const size_t m_BlocksPerChunk;
    const size_t m_MaxCacheSize;

    std::mutex m_ChunkMutex;
    std::vector<std::unique_ptr<std::byte[]>> m_Chunks;
    std::atomic<FreeNode *> m_GlobalFreeList{nullptr};
};

} // namespace BML::Core

#endif // BML_CORE_FIXED_BLOCK_POOL_H
