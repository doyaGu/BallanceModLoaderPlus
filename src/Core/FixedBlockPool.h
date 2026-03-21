#ifndef BML_CORE_FIXED_BLOCK_POOL_H
#define BML_CORE_FIXED_BLOCK_POOL_H

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <thread>
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
        explicit FixedBlockPool(size_t blockSize, size_t blocksPerChunk = 1024, size_t threadCacheSize = 64)
            : m_BlockSize(blockSize),
              m_BlockStride(AlignUp(std::max(blockSize, sizeof(FreeNode)), alignof(std::max_align_t))),
              m_BlocksPerChunk(std::max<size_t>(blocksPerChunk, 1)),
              m_MaxCacheSize(std::max<size_t>(threadCacheSize, 8)) {
            AllocateChunk();
        }

        ~FixedBlockPool() {
            m_ShuttingDown.store(true, std::memory_order_release);
            while (m_ActiveOps.load(std::memory_order_acquire) != 0) {
                std::this_thread::yield();
            }

            auto &caches = ThreadCaches();
            caches.erase(
                std::remove_if(
                    caches.begin(),
                    caches.end(),
                    [this](const CacheEntry &entry) {
                        auto token = entry.token.lock();
                        return entry.owner == this && token && token == m_CacheToken;
                    }),
                caches.end());
        }

        FixedBlockPool(const FixedBlockPool &) = delete;
        FixedBlockPool &operator=(const FixedBlockPool &) = delete;

        void *Allocate() {
            OperationGuard operation(*this);
            if (!operation)
                return nullptr;

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
        T *Construct(Args &&... args) {
            if (sizeof(T) > m_BlockSize)
                return nullptr;
            void *memory = Allocate();
            if (!memory)
                return nullptr;
            return new(memory) T(std::forward<Args>(args)...);
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

            OperationGuard operation(*this);
            if (!operation)
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

        struct CacheToken {};

        struct CacheEntry {
            FixedBlockPool *owner{nullptr};
            std::weak_ptr<CacheToken> token;
            ThreadCache cache{};
        };

        class OperationGuard {
        public:
            explicit OperationGuard(FixedBlockPool &pool)
                : m_Pool(&pool) {
                for (;;) {
                    if (m_Pool->m_ShuttingDown.load(std::memory_order_acquire))
                        return;

                    m_Pool->m_ActiveOps.fetch_add(1, std::memory_order_acq_rel);
                    if (!m_Pool->m_ShuttingDown.load(std::memory_order_acquire)) {
                        m_Active = true;
                        return;
                    }
                    m_Pool->m_ActiveOps.fetch_sub(1, std::memory_order_acq_rel);
                }
            }

            ~OperationGuard() {
                if (m_Active)
                    m_Pool->m_ActiveOps.fetch_sub(1, std::memory_order_acq_rel);
            }

            explicit operator bool() const noexcept { return m_Active; }

            OperationGuard(const OperationGuard &) = delete;
            OperationGuard &operator=(const OperationGuard &) = delete;

        private:
            FixedBlockPool *m_Pool{nullptr};
            bool m_Active{false};
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

        static std::vector<CacheEntry> &ThreadCaches() {
            static thread_local std::vector<CacheEntry> caches;
            return caches;
        }

        ThreadCache &GetThreadCache() {
            auto &caches = ThreadCaches();
            caches.erase(
                std::remove_if(
                    caches.begin(),
                    caches.end(),
                    [](const CacheEntry &entry) { return entry.token.expired(); }),
                caches.end());

            for (auto &entry : caches) {
                auto token = entry.token.lock();
                if (entry.owner == this && token && token == m_CacheToken) {
                    return entry.cache;
                }
            }
            caches.push_back(CacheEntry{this, m_CacheToken, {}});
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
                new(node) FreeNode{};
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
        std::shared_ptr<CacheToken> m_CacheToken{std::make_shared<CacheToken>()};
        std::atomic<bool> m_ShuttingDown{false};
        std::atomic<size_t> m_ActiveOps{0};

        std::mutex m_ChunkMutex;
        std::vector<std::unique_ptr<std::byte[]>> m_Chunks;
        std::atomic<FreeNode *> m_GlobalFreeList{nullptr};
    };
} // namespace BML::Core

#endif // BML_CORE_FIXED_BLOCK_POOL_H
