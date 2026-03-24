/**
 * @file bml_memory.hpp
 * @brief BML C++ Memory Management Wrapper
 *
 * Provides RAII-friendly and type-safe wrappers for BML memory management
 * including allocation, memory pools, and smart pointer utilities.
 */

#ifndef BML_MEMORY_HPP
#define BML_MEMORY_HPP

#include "bml_memory.h"
#include "bml_errors.h"
#include "bml_assert.hpp"

#include <cstddef>
#include <optional>
#include <memory>
#include <type_traits>

namespace bml {
    // ============================================================================
    // Basic Allocation Functions
    // ============================================================================

    /**
     * @brief Allocate memory using BML allocator
     * @param size Number of bytes to allocate
     * @return Pointer to allocated memory, or nullptr on failure
     */
    inline void *Alloc(size_t size, const BML_CoreMemoryInterface *memoryInterface = nullptr) {
        return memoryInterface ? memoryInterface->Alloc(memoryInterface->Context, size) : nullptr;
    }

    /**
     * @brief Allocate zero-initialized memory
     * @param count Number of elements
     * @param size Size of each element
     * @return Pointer to allocated memory, or nullptr on failure
     */
    inline void *Calloc(size_t count, size_t size, const BML_CoreMemoryInterface *memoryInterface = nullptr) {
        return memoryInterface ? memoryInterface->Calloc(memoryInterface->Context, count, size) : nullptr;
    }

    /**
     * @brief Resize allocated memory
     * @param ptr Pointer to memory
     * @param old_size Original size in bytes
     * @param new_size New size in bytes
     * @return Pointer to resized memory, or nullptr on failure
     */
    inline void *Realloc(void *ptr,
                         size_t old_size,
                         size_t new_size,
                         const BML_CoreMemoryInterface *memoryInterface = nullptr) {
        return memoryInterface ? memoryInterface->Realloc(memoryInterface->Context, ptr, old_size, new_size) : nullptr;
    }

    /**
     * @brief Free memory allocated by BML
     * @param ptr Pointer to memory
     */
    inline void Free(void *ptr, const BML_CoreMemoryInterface *memoryInterface = nullptr) {
        if (memoryInterface) {
            memoryInterface->Free(memoryInterface->Context, ptr);
        }
    }

    /**
     * @brief Allocate aligned memory
     * @param size Number of bytes to allocate
     * @param alignment Alignment in bytes (must be power of 2)
     * @return Pointer to aligned memory, or nullptr on failure
     */
    inline void *AllocAligned(size_t size,
                              size_t alignment,
                              const BML_CoreMemoryInterface *memoryInterface = nullptr) {
        return memoryInterface ? memoryInterface->AllocAligned(memoryInterface->Context, size, alignment) : nullptr;
    }

    /**
     * @brief Free aligned memory
     * @param ptr Pointer allocated by AllocAligned
     */
    inline void FreeAligned(void *ptr, const BML_CoreMemoryInterface *memoryInterface = nullptr) {
        if (memoryInterface) {
            memoryInterface->FreeAligned(memoryInterface->Context, ptr);
        }
    }

    // ============================================================================
    // Memory Statistics
    // ============================================================================

    inline std::optional<BML_MemoryStats> GetMemoryStats(
        const BML_CoreMemoryInterface *memoryInterface = nullptr) {
        if (!memoryInterface) return std::nullopt;
        BML_MemoryStats stats{};
        if (memoryInterface->GetMemoryStats(memoryInterface->Context, &stats) == BML_RESULT_OK) {
            return stats;
        }
        return std::nullopt;
    }

    // ============================================================================
    // BML Deleter for Smart Pointers
    // ============================================================================

    /**
     * @brief Deleter for use with std::unique_ptr for BML-allocated memory
     */
    struct BmlDeleter {
        const BML_CoreMemoryInterface *m_MemoryInterface = nullptr;

        void operator()(void *ptr) const noexcept {
            if (m_MemoryInterface) {
                m_MemoryInterface->Free(m_MemoryInterface->Context, ptr);
            }
        }
    };

    /**
     * @brief Deleter for aligned BML-allocated memory
     */
    struct BmlAlignedDeleter {
        const BML_CoreMemoryInterface *m_MemoryInterface = nullptr;

        void operator()(void *ptr) const noexcept {
            if (m_MemoryInterface) {
                m_MemoryInterface->FreeAligned(m_MemoryInterface->Context, ptr);
            }
        }
    };

    /**
     * @brief Typed deleter that calls destructor then frees with BML
     */
    template <typename T>
    struct BmlTypedDeleter {
        const BML_CoreMemoryInterface *m_MemoryInterface = nullptr;

        void operator()(T *ptr) const noexcept {
            if (ptr) {
                ptr->~T();
                if (m_MemoryInterface) {
                    m_MemoryInterface->Free(m_MemoryInterface->Context, ptr);
                }
            }
        }
    };

    // ============================================================================
    // Smart Pointer Aliases
    // ============================================================================

    /**
     * @brief Unique pointer with BML memory management
     */
    template <typename T>
    using UniquePtr = std::unique_ptr<T, BmlTypedDeleter<T>>;

    /**
     * @brief Create a unique pointer with BML allocation
     * @tparam T Object type
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     * @return UniquePtr<T> or nullptr on allocation failure
     */
    template <typename T, typename... Args>
    UniquePtr<T> MakeUnique(Args &&... args) {
        void *mem = Alloc(sizeof(T));
        if (!mem) return nullptr;

        T *obj = new(mem) T(std::forward<Args>(args)...);
        return UniquePtr<T>(obj, BmlTypedDeleter<T>{nullptr});
    }

    template <typename T, typename... Args>
    UniquePtr<T> MakeUniqueWithInterface(
        const BML_CoreMemoryInterface *memoryInterface,
        Args &&... args) {
        void *mem = Alloc(sizeof(T), memoryInterface);
        if (!mem) return nullptr;

        T *obj = new(mem) T(std::forward<Args>(args)...);
        return UniquePtr<T>(obj, BmlTypedDeleter<T>{memoryInterface});
    }

    /**
     * @brief Deleter for BML-allocated arrays that calls element destructors
     */
    template <typename T>
    struct BmlArrayDeleter {
        size_t count;
        const BML_CoreMemoryInterface *m_MemoryInterface = nullptr;

        void operator()(T *ptr) const noexcept {
            if (ptr) {
                for (size_t i = 0; i < count; ++i) {
                    ptr[i].~T();
                }
                if (m_MemoryInterface) {
                    m_MemoryInterface->Free(m_MemoryInterface->Context, ptr);
                }
            }
        }
    };

    /**
     * @brief Create a unique pointer for array with BML allocation
     * @tparam T Element type
     * @param count Number of elements
     * @return unique_ptr or nullptr on allocation failure
     */
    template <typename T>
    std::unique_ptr<T[], BmlArrayDeleter<T>> MakeUniqueArray(size_t count) {
        void *mem = Calloc(count, sizeof(T));
        if (!mem) return nullptr;

        // Default-construct elements
        T *arr = static_cast<T *>(mem);
        for (size_t i = 0; i < count; ++i) {
            new(&arr[i]) T();
        }

        return std::unique_ptr<T[], BmlArrayDeleter<T>>(
            arr,
            BmlArrayDeleter<T>{count, nullptr});
    }

    template <typename T>
    std::unique_ptr<T[], BmlArrayDeleter<T>> MakeUniqueArrayWithInterface(
        size_t count,
        const BML_CoreMemoryInterface *memoryInterface) {
        void *mem = Calloc(count, sizeof(T), memoryInterface);
        if (!mem) return nullptr;

        T *arr = static_cast<T *>(mem);
        for (size_t i = 0; i < count; ++i) {
            new(&arr[i]) T();
        }

        return std::unique_ptr<T[], BmlArrayDeleter<T>>(
            arr,
            BmlArrayDeleter<T>{count, memoryInterface});
    }

    // ============================================================================
    // Memory Pool Wrapper
    // ============================================================================

    /**
     * @brief RAII wrapper for BML_MemoryPool
     *
     * Example:
     *   bml::MemoryPool pool(sizeof(MyObject), 100);
     *   MyObject* obj = pool.Alloc<MyObject>();
     *   pool.Free(obj);
     */
    class MemoryPool {
    public:
        /**
         * @brief Create a memory pool
         * @param block_size Size of each block in bytes
         * @param initial_blocks Number of blocks to pre-allocate
         * @note On failure, the pool is left empty (operator bool() returns false)
         */
        explicit MemoryPool(size_t block_size,
                            uint32_t initial_blocks = 32,
                            const BML_CoreMemoryInterface *memoryInterface = nullptr)
            : m_Handle(nullptr), m_BlockSize(block_size), m_MemoryInterface(memoryInterface) {
            BML_ASSERT(memoryInterface);
            auto result = m_MemoryInterface->MemoryPoolCreate(
                m_MemoryInterface->Context, block_size, initial_blocks, &m_Handle);
            // If failed, m_Handle stays nullptr — caller checks operator bool()
            (void)result;
        }

        ~MemoryPool() {
            if (m_Handle) m_MemoryInterface->MemoryPoolDestroy(m_MemoryInterface->Context, m_Handle);
        }

        // Non-copyable
        MemoryPool(const MemoryPool &) = delete;
        MemoryPool &operator=(const MemoryPool &) = delete;

        // Movable
        MemoryPool(MemoryPool &&other) noexcept
            : m_Handle(other.m_Handle),
              m_BlockSize(other.m_BlockSize),
              m_MemoryInterface(other.m_MemoryInterface) {
            other.m_Handle = nullptr;
            other.m_MemoryInterface = nullptr;
        }

        MemoryPool &operator=(MemoryPool &&other) noexcept {
            if (this != &other) {
                if (m_Handle) m_MemoryInterface->MemoryPoolDestroy(m_MemoryInterface->Context, m_Handle);
                m_Handle = other.m_Handle;
                m_BlockSize = other.m_BlockSize;
                m_MemoryInterface = other.m_MemoryInterface;
                other.m_Handle = nullptr;
                other.m_MemoryInterface = nullptr;
            }
            return *this;
        }

        /**
         * @brief Allocate a block from the pool
         * @return Pointer to block, or nullptr on failure
         */
        void *Alloc() {
            return m_MemoryInterface->MemoryPoolAlloc(m_MemoryInterface->Context, m_Handle);
        }

        /**
         * @brief Allocate and construct an object
         * @tparam T Object type
         * @tparam Args Constructor argument types
         * @param args Constructor arguments
         * @return Pointer to constructed object, or nullptr on failure
         */
        template <typename T, typename... Args>
        T *Alloc(Args &&... args) {
            static_assert(sizeof(T) <= 0xFFFFFFFF, "Type too large");
            void *mem = Alloc();
            if (!mem) return nullptr;
            return new(mem) T(std::forward<Args>(args)...);
        }

        /**
         * @brief Return a block to the pool
         * @param ptr Pointer allocated from this pool
         */
        void Free(void *ptr) {
            m_MemoryInterface->MemoryPoolFree(m_MemoryInterface->Context, m_Handle, ptr);
        }

        /**
         * @brief Destruct an object and return block to pool
         * @tparam T Object type
         * @param ptr Pointer to object
         */
        template <typename T>
        void Free(T *ptr) {
            if (ptr) {
                ptr->~T();
                Free(static_cast<void *>(ptr));
            }
        }

        /**
         * @brief Get block size
         */
        size_t BlockSize() const noexcept { return m_BlockSize; }

        /**
         * @brief Check if pool was created successfully
         */
        explicit operator bool() const noexcept { return m_Handle != nullptr; }

        /**
         * @brief Get the underlying handle
         */
        BML_MemoryPool Handle() const noexcept { return m_Handle; }

    private:
        BML_MemoryPool m_Handle;
        size_t m_BlockSize;
        const BML_CoreMemoryInterface *m_MemoryInterface;
    };

    // ============================================================================
    // Pool Object (RAII object allocated from pool)
    // ============================================================================

    /**
     * @brief RAII wrapper for objects allocated from a memory pool
     *
     * Example:
     *   bml::MemoryPool pool(sizeof(MyObject), 100);
     *   auto obj = bml::PoolObject<MyObject>::create(pool, arg1, arg2);
     *   // Object is automatically freed when obj goes out of scope
     */
    template <typename T>
    class PoolObject {
    public:
        /**
         * @brief Create a pool-allocated object
         * @param pool Memory pool to allocate from
         * @param args Constructor arguments
         * @return PoolObject containing the object
         */
        template <typename... Args>
        static PoolObject create(MemoryPool &pool, Args &&... args) {
            T *ptr = pool.Alloc<T>(std::forward<Args>(args)...);
            return PoolObject(&pool, ptr);
        }

        PoolObject() : m_Pool(nullptr), m_Ptr(nullptr) {}

        ~PoolObject() {
            Reset();
        }

        // Non-copyable
        PoolObject(const PoolObject &) = delete;
        PoolObject &operator=(const PoolObject &) = delete;

        // Movable
        PoolObject(PoolObject &&other) noexcept
            : m_Pool(other.m_Pool), m_Ptr(other.m_Ptr) {
            other.m_Pool = nullptr;
            other.m_Ptr = nullptr;
        }

        PoolObject &operator=(PoolObject &&other) noexcept {
            if (this != &other) {
                Reset();
                m_Pool = other.m_Pool;
                m_Ptr = other.m_Ptr;
                other.m_Pool = nullptr;
                other.m_Ptr = nullptr;
            }
            return *this;
        }

        /**
         * @brief Reset and free the object
         */
        void Reset() {
            if (m_Ptr && m_Pool) {
                m_Pool->Free(m_Ptr);
            }
            m_Ptr = nullptr;
            m_Pool = nullptr;
        }

        T *Get() const noexcept { return m_Ptr; }
        T *operator->() const noexcept { return m_Ptr; }
        T &operator*() const noexcept { return *m_Ptr; }

        explicit operator bool() const noexcept { return m_Ptr != nullptr; }

    private:
        PoolObject(MemoryPool *pool, T *ptr) : m_Pool(pool), m_Ptr(ptr) {}

        MemoryPool *m_Pool;
        T *m_Ptr;
    };
} // namespace bml

#endif /* BML_MEMORY_HPP */

