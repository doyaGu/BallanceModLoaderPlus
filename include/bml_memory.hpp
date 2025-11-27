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

#include <cstddef>
#include <optional>
#include <memory>
#include <type_traits>

namespace bml {

// ============================================================================
// Memory Capabilities Query
// ============================================================================

/**
 * @brief Query memory subsystem capabilities
 * @return Capabilities if successful
 */
inline std::optional<BML_MemoryCaps> GetMemoryCaps() {
    if (!bmlGetMemoryCaps) return std::nullopt;
    BML_MemoryCaps caps{};
    caps.struct_size = sizeof(BML_MemoryCaps);
    if (bmlGetMemoryCaps(&caps) == BML_RESULT_OK) {
        return caps;
    }
    return std::nullopt;
}

/**
 * @brief Check if a memory capability is available
 * @param flag Capability flag to check
 * @return true if capability is available
 */
inline bool HasMemoryCap(BML_MemoryCapabilityFlags flag) {
    auto caps = GetMemoryCaps();
    return caps && (caps->capability_flags & flag);
}

/**
 * @brief Get memory allocation statistics
 * @return Statistics if available
 */
inline std::optional<BML_MemoryStats> GetMemoryStats() {
    if (!bmlGetMemoryStats) return std::nullopt;
    BML_MemoryStats stats{};
    if (bmlGetMemoryStats(&stats) == BML_RESULT_OK) {
        return stats;
    }
    return std::nullopt;
}

// ============================================================================
// Basic Allocation Functions
// ============================================================================

/**
 * @brief Allocate memory using BML allocator
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or nullptr on failure
 */
inline void* Alloc(size_t size) {
    return bmlAlloc ? bmlAlloc(size) : nullptr;
}

/**
 * @brief Allocate zero-initialized memory
 * @param count Number of elements
 * @param size Size of each element
 * @return Pointer to allocated memory, or nullptr on failure
 */
inline void* Calloc(size_t count, size_t size) {
    return bmlCalloc ? bmlCalloc(count, size) : nullptr;
}

/**
 * @brief Resize allocated memory
 * @param ptr Pointer to memory
 * @param new_size New size in bytes
 * @return Pointer to resized memory, or nullptr on failure
 */
inline void* Realloc(void* ptr, size_t new_size) {
    return bmlRealloc ? bmlRealloc(ptr, new_size) : nullptr;
}

/**
 * @brief Free memory allocated by BML
 * @param ptr Pointer to memory
 */
inline void Free(void* ptr) {
    if (bmlFree) bmlFree(ptr);
}

/**
 * @brief Allocate aligned memory
 * @param size Number of bytes to allocate
 * @param alignment Alignment in bytes (must be power of 2)
 * @return Pointer to aligned memory, or nullptr on failure
 */
inline void* AllocAligned(size_t size, size_t alignment) {
    return bmlAllocAligned ? bmlAllocAligned(size, alignment) : nullptr;
}

/**
 * @brief Free aligned memory
 * @param ptr Pointer allocated by AllocAligned
 */
inline void FreeAligned(void* ptr) {
    if (bmlFreeAligned) bmlFreeAligned(ptr);
}

// ============================================================================
// BML Deleter for Smart Pointers
// ============================================================================

/**
 * @brief Deleter for use with std::unique_ptr for BML-allocated memory
 */
struct BmlDeleter {
    void operator()(void* ptr) const noexcept {
        if (bmlFree) bmlFree(ptr);
    }
};

/**
 * @brief Deleter for aligned BML-allocated memory
 */
struct BmlAlignedDeleter {
    void operator()(void* ptr) const noexcept {
        if (bmlFreeAligned) bmlFreeAligned(ptr);
    }
};

/**
 * @brief Typed deleter that calls destructor then frees with BML
 */
template<typename T>
struct BmlTypedDeleter {
    void operator()(T* ptr) const noexcept {
        if (ptr) {
            ptr->~T();
            if (bmlFree) bmlFree(ptr);
        }
    }
};

// ============================================================================
// Smart Pointer Aliases
// ============================================================================

/**
 * @brief Unique pointer with BML memory management
 */
template<typename T>
using UniquePtr = std::unique_ptr<T, BmlTypedDeleter<T>>;

/**
 * @brief Create a unique pointer with BML allocation
 * @tparam T Object type
 * @tparam Args Constructor argument types
 * @param args Constructor arguments
 * @return UniquePtr<T> or nullptr on allocation failure
 */
template<typename T, typename... Args>
UniquePtr<T> MakeUnique(Args&&... args) {
    void* mem = bmlAlloc ? bmlAlloc(sizeof(T)) : nullptr;
    if (!mem) return nullptr;
    
    T* obj = new(mem) T(std::forward<Args>(args)...);
    return UniquePtr<T>(obj);
}

/**
 * @brief Create a unique pointer for array with BML allocation
 * @tparam T Element type
 * @param count Number of elements
 * @return UniquePtr<T[]> or nullptr on allocation failure
 */
template<typename T>
std::unique_ptr<T[], BmlDeleter> MakeUniqueArray(size_t count) {
    void* mem = bmlCalloc ? bmlCalloc(count, sizeof(T)) : nullptr;
    if (!mem) return nullptr;
    
    // Default-construct elements
    T* arr = static_cast<T*>(mem);
    for (size_t i = 0; i < count; ++i) {
        new(&arr[i]) T();
    }
    
    return std::unique_ptr<T[], BmlDeleter>(arr);
}

// ============================================================================
// Memory Pool Wrapper
// ============================================================================

/**
 * @brief RAII wrapper for BML_MemoryPool
 * 
 * Example:
 *   bml::MemoryPool pool(sizeof(MyObject), 100);
 *   MyObject* obj = pool.alloc<MyObject>();
 *   pool.free(obj);
 */
class MemoryPool {
public:
    /**
     * @brief Create a memory pool
     * @param block_size Size of each block in bytes
     * @param initial_blocks Number of blocks to pre-allocate
     * @throws bml::Exception if creation fails
     */
    MemoryPool(size_t block_size, uint32_t initial_blocks = 32)
        : m_handle(nullptr), m_block_size(block_size)
    {
        if (!bmlMemoryPoolCreate) {
            throw Exception(BML_RESULT_NOT_FOUND, "MemoryPool API unavailable");
        }
        auto result = bmlMemoryPoolCreate(block_size, initial_blocks, &m_handle);
        if (result != BML_RESULT_OK) {
            throw Exception(result, "Failed to create memory pool");
        }
    }
    
    ~MemoryPool() {
        if (m_handle && bmlMemoryPoolDestroy) {
            bmlMemoryPoolDestroy(m_handle);
        }
    }
    
    // Non-copyable
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
    
    // Movable
    MemoryPool(MemoryPool&& other) noexcept
        : m_handle(other.m_handle), m_block_size(other.m_block_size)
    {
        other.m_handle = nullptr;
    }
    
    MemoryPool& operator=(MemoryPool&& other) noexcept {
        if (this != &other) {
            if (m_handle && bmlMemoryPoolDestroy) {
                bmlMemoryPoolDestroy(m_handle);
            }
            m_handle = other.m_handle;
            m_block_size = other.m_block_size;
            other.m_handle = nullptr;
        }
        return *this;
    }
    
    /**
     * @brief Allocate a block from the pool
     * @return Pointer to block, or nullptr on failure
     */
    void* alloc() {
        return bmlMemoryPoolAlloc ? bmlMemoryPoolAlloc(m_handle) : nullptr;
    }
    
    /**
     * @brief Allocate and construct an object
     * @tparam T Object type
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     * @return Pointer to constructed object, or nullptr on failure
     */
    template<typename T, typename... Args>
    T* alloc(Args&&... args) {
        static_assert(sizeof(T) <= 0xFFFFFFFF, "Type too large");
        void* mem = alloc();
        if (!mem) return nullptr;
        return new(mem) T(std::forward<Args>(args)...);
    }
    
    /**
     * @brief Return a block to the pool
     * @param ptr Pointer allocated from this pool
     */
    void free(void* ptr) {
        if (bmlMemoryPoolFree) bmlMemoryPoolFree(m_handle, ptr);
    }
    
    /**
     * @brief Destruct an object and return block to pool
     * @tparam T Object type
     * @param ptr Pointer to object
     */
    template<typename T>
    void free(T* ptr) {
        if (ptr) {
            ptr->~T();
            free(static_cast<void*>(ptr));
        }
    }
    
    /**
     * @brief Get block size
     */
    size_t block_size() const noexcept { return m_block_size; }
    
    /**
     * @brief Get the underlying handle
     */
    BML_MemoryPool handle() const noexcept { return m_handle; }
    
private:
    BML_MemoryPool m_handle;
    size_t m_block_size;
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
template<typename T>
class PoolObject {
public:
    /**
     * @brief Create a pool-allocated object
     * @param pool Memory pool to allocate from
     * @param args Constructor arguments
     * @return PoolObject containing the object
     */
    template<typename... Args>
    static PoolObject create(MemoryPool& pool, Args&&... args) {
        T* ptr = pool.alloc<T>(std::forward<Args>(args)...);
        return PoolObject(&pool, ptr);
    }
    
    PoolObject() : m_pool(nullptr), m_ptr(nullptr) {}
    
    ~PoolObject() {
        reset();
    }
    
    // Non-copyable
    PoolObject(const PoolObject&) = delete;
    PoolObject& operator=(const PoolObject&) = delete;
    
    // Movable
    PoolObject(PoolObject&& other) noexcept
        : m_pool(other.m_pool), m_ptr(other.m_ptr)
    {
        other.m_pool = nullptr;
        other.m_ptr = nullptr;
    }
    
    PoolObject& operator=(PoolObject&& other) noexcept {
        if (this != &other) {
            reset();
            m_pool = other.m_pool;
            m_ptr = other.m_ptr;
            other.m_pool = nullptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }
    
    /**
     * @brief Reset and free the object
     */
    void reset() {
        if (m_ptr && m_pool) {
            m_pool->free(m_ptr);
        }
        m_ptr = nullptr;
        m_pool = nullptr;
    }
    
    T* get() const noexcept { return m_ptr; }
    T* operator->() const noexcept { return m_ptr; }
    T& operator*() const noexcept { return *m_ptr; }
    
    explicit operator bool() const noexcept { return m_ptr != nullptr; }
    
private:
    PoolObject(MemoryPool* pool, T* ptr) : m_pool(pool), m_ptr(ptr) {}
    
    MemoryPool* m_pool;
    T* m_ptr;
};

} // namespace bml

#endif /* BML_MEMORY_HPP */
