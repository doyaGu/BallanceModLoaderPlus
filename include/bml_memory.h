#ifndef BML_MEMORY_H
#define BML_MEMORY_H

/**
 * @file bml_memory.h
 * @brief Memory management API for safe cross-DLL allocations
 * 
 * This API provides unified memory allocation to solve cross-DLL boundary issues
 * where memory allocated in one module (with its CRT) cannot be safely freed in
 * another module (with a different CRT).
 * 
 * All memory allocated through BML APIs (e.g., bmlImcCallRpcSync response buffers)
 * must be freed using bmlFree().
 */

#include "bml_types.h"
#include "bml_errors.h"
#include "bml_version.h"

BML_BEGIN_CDECLS

/* ========== Basic Memory Allocation ========== */

/**
 * @brief Allocate memory from BML's unified allocator
 * 
 * @param[in] size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL on failure
 * 
 * @threadsafe Yes
 * 
 * @note Memory must be freed with bmlFree()
 * @note Returns NULL if size is 0
 * @note Allocated memory is uninitialized
 */
typedef void *(*PFN_BML_Alloc)(size_t size);

/**
 * @brief Allocate zero-initialized memory
 * 
 * @param[in] count Number of elements
 * @param[in] size Size of each element in bytes
 * @return Pointer to allocated memory, or NULL on failure
 * 
 * @threadsafe Yes
 * 
 * @note Memory is zero-initialized
 * @note Returns NULL if count * size overflows or is 0
 */
typedef void *(*PFN_BML_Calloc)(size_t count, size_t size);

/**
 * @brief Resize previously allocated memory
 * 
 * @param[in] ptr Pointer to memory allocated by bmlAlloc/bmlCalloc/bmlRealloc
 * @param[in] new_size New size in bytes
 * @return Pointer to resized memory, or NULL on failure
 * 
 * @threadsafe Yes
 * 
 * @note If ptr is NULL, behaves like bmlAlloc(new_size)
 * @note If new_size is 0, behaves like bmlFree(ptr) and returns NULL
 * @note Original content is preserved up to min(old_size, new_size)
 * @note If reallocation fails, original pointer remains valid
 */
typedef void *(*PFN_BML_Realloc)(void *ptr, size_t new_size);

/**
 * @brief Free memory allocated by BML
 * 
 * @param[in] ptr Pointer to memory allocated by bmlAlloc/bmlCalloc/bmlRealloc
 * 
 * @threadsafe Yes
 * 
 * @note Safe to call with NULL pointer (no-op)
 * @note Pointer becomes invalid after this call
 */
typedef void (*PFN_BML_Free)(void *ptr);

/**
 * @brief Allocate aligned memory
 * 
 * @param[in] size Number of bytes to allocate
 * @param[in] alignment Alignment in bytes (must be power of 2)
 * @return Pointer to aligned memory, or NULL on failure
 * 
 * @threadsafe Yes
 * 
 * @note Memory must be freed with bmlFreeAligned()
 * @note Returns NULL if alignment is not power of 2
 */
typedef void *(*PFN_BML_AllocAligned)(size_t size, size_t alignment);

/**
 * @brief Free aligned memory
 * 
 * @param[in] ptr Pointer allocated by bmlAllocAligned
 * 
 * @threadsafe Yes
 */
typedef void (*PFN_BML_FreeAligned)(void *ptr);

/* ========== Memory Pool ========== */

/**
 * @brief Opaque handle to a memory pool
 * 
 * Memory pools are useful for high-frequency allocations of same-sized objects.
 * They reduce fragmentation and improve performance.
 */
typedef struct BML_MemoryPool_T *BML_MemoryPool;

/**
 * @brief Create a memory pool for fixed-size blocks
 * 
 * @param[in] block_size Size of each block in bytes
 * @param[in] initial_blocks Number of blocks to pre-allocate
 * @param[out] out_pool Receives pool handle
 * @return BML_RESULT_OK on success
 * 
 * @threadsafe Yes
 * 
 * @note Pool grows automatically when exhausted
 * @note Destroy pool with bmlMemoryPoolDestroy()
 */
typedef BML_Result (*PFN_BML_MemoryPoolCreate)(
    size_t block_size,
    uint32_t initial_blocks,
    BML_MemoryPool *out_pool
);

/**
 * @brief Allocate a block from the pool
 * 
 * @param[in] pool Pool handle
 * @return Pointer to block, or NULL on failure
 * 
 * @threadsafe Yes (lock-free for single producer/consumer)
 */
typedef void *(*PFN_BML_MemoryPoolAlloc)(BML_MemoryPool pool);

/**
 * @brief Return a block to the pool
 * 
 * @param[in] pool Pool handle
 * @param[in] ptr Pointer allocated from this pool
 * 
 * @threadsafe Yes
 */
typedef void (*PFN_BML_MemoryPoolFree)(BML_MemoryPool pool, void *ptr);

/**
 * @brief Destroy a memory pool
 * 
 * @param[in] pool Pool handle
 * 
 * @threadsafe No (ensure no concurrent allocs/frees)
 * 
 * @note All blocks are freed automatically
 * @note Outstanding pointers become invalid
 */
typedef void (*PFN_BML_MemoryPoolDestroy)(BML_MemoryPool pool);

/* ========== Memory Statistics ========== */

/**
 * @brief Memory allocation statistics
 */
typedef struct BML_MemoryStats {
    uint64_t total_allocated;      /**< Total bytes currently allocated */
    uint64_t peak_allocated;       /**< Peak allocation in bytes */
    uint64_t total_alloc_count;    /**< Total number of allocations */
    uint64_t total_free_count;     /**< Total number of frees */
    uint64_t active_alloc_count;   /**< Currently active allocations */
} BML_MemoryStats;

/**
 * @brief Get memory allocation statistics
 * 
 * @param[out] out_stats Receives statistics
 * @return BML_RESULT_OK on success
 * 
 * @threadsafe Yes
 * 
 * @note Only available if BML built with memory tracking
 */
typedef BML_Result (*PFN_BML_GetMemoryStats)(BML_MemoryStats *out_stats);

/* ========== Capability Query ========== */

typedef enum BML_MemoryCapabilityFlags {
    BML_MEMORY_CAP_BASIC_ALLOC      = 1u << 0,  /**< Basic alloc/free */
    BML_MEMORY_CAP_ALIGNED_ALLOC    = 1u << 1,  /**< Aligned allocation */
    BML_MEMORY_CAP_MEMORY_POOLS     = 1u << 2,  /**< Memory pool support */
    BML_MEMORY_CAP_STATISTICS       = 1u << 3,  /**< Memory stats tracking */
    BML_MEMORY_CAP_DEBUG_INFO       = 1u << 4   /**< Debug allocation info */
} BML_MemoryCapabilityFlags;

typedef struct BML_MemoryCaps {
    uint32_t struct_size;
    BML_Version api_version;
    uint32_t capability_flags;
    size_t default_alignment;      /**< Default memory alignment in bytes */
    size_t min_pool_block_size;    /**< Minimum pool block size */
    size_t max_pool_block_size;    /**< Maximum pool block size */
    BML_ThreadingModel threading_model;
} BML_MemoryCaps;

typedef BML_Result (*PFN_BML_MemoryGetCaps)(BML_MemoryCaps *out_caps);

/* ========== Global Function Pointers ========== */

extern PFN_BML_Alloc              bmlAlloc;
extern PFN_BML_Calloc             bmlCalloc;
extern PFN_BML_Realloc            bmlRealloc;
extern PFN_BML_Free               bmlFree;
extern PFN_BML_AllocAligned       bmlAllocAligned;
extern PFN_BML_FreeAligned        bmlFreeAligned;

extern PFN_BML_MemoryPoolCreate   bmlMemoryPoolCreate;
extern PFN_BML_MemoryPoolAlloc    bmlMemoryPoolAlloc;
extern PFN_BML_MemoryPoolFree     bmlMemoryPoolFree;
extern PFN_BML_MemoryPoolDestroy  bmlMemoryPoolDestroy;

extern PFN_BML_GetMemoryStats     bmlGetMemoryStats;
extern PFN_BML_MemoryGetCaps         bmlMemoryGetCaps;

BML_END_CDECLS

#endif /* BML_MEMORY_H */
