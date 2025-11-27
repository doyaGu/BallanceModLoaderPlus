#include "ApiRegistrationMacros.h"
#include "MemoryManager.h"
#include "bml_capabilities.h"

#include "bml_memory.h"

namespace BML::Core {
    void *BML_API_Alloc(size_t size) {
        return MemoryManager::Instance().Alloc(size);
    }

    void *BML_API_Calloc(size_t count, size_t size) {
        return MemoryManager::Instance().Calloc(count, size);
    }

    void *BML_API_Realloc(void *ptr, size_t new_size) {
        // Legacy API - uses inaccurate tracking
        return MemoryManager::Instance().ReallocUnknownSize(ptr, new_size);
    }

    void *BML_API_ReallocEx(void *ptr, size_t old_size, size_t new_size) {
        return MemoryManager::Instance().Realloc(ptr, old_size, new_size);
    }

    void BML_API_Free(void *ptr) {
        MemoryManager::Instance().Free(ptr);
    }

    void BML_API_FreeWithSize(void *ptr, size_t size) {
        MemoryManager::Instance().FreeWithSize(ptr, size);
    }

    void *BML_API_AllocAligned(size_t size, size_t alignment) {
        return MemoryManager::Instance().AllocAligned(size, alignment);
    }

    void BML_API_FreeAligned(void *ptr) {
        MemoryManager::Instance().FreeAligned(ptr);
    }

    BML_Result BML_API_MemoryPoolCreate(size_t block_size, uint32_t initial_blocks, BML_MemoryPool *out_pool) {
        return MemoryManager::Instance().CreatePool(block_size, initial_blocks, out_pool);
    }

    void *BML_API_MemoryPoolAlloc(BML_MemoryPool pool) {
        return MemoryManager::Instance().PoolAlloc(pool);
    }

    void BML_API_MemoryPoolFree(BML_MemoryPool pool, void *ptr) {
        MemoryManager::Instance().PoolFree(pool, ptr);
    }

    void BML_API_MemoryPoolDestroy(BML_MemoryPool pool) {
        MemoryManager::Instance().DestroyPool(pool);
    }

    BML_Result BML_API_GetMemoryStats(BML_MemoryStats *out_stats) {
        return MemoryManager::Instance().GetStats(out_stats);
    }

    BML_Result BML_API_GetMemoryCaps(BML_MemoryCaps *out_caps) {
        return MemoryManager::Instance().GetCaps(out_caps);
    }

    void RegisterMemoryApis() {
        BML_BEGIN_API_REGISTRATION();

        /* Basic allocation - direct functions, no error guard */
        BML_REGISTER_API_WITH_CAPS(bmlAlloc, BML_API_Alloc, BML_CAP_MEMORY_BASIC);
        BML_REGISTER_API_WITH_CAPS(bmlCalloc, BML_API_Calloc, BML_CAP_MEMORY_BASIC);
        BML_REGISTER_API_WITH_CAPS(bmlRealloc, BML_API_Realloc, BML_CAP_MEMORY_BASIC);
        BML_REGISTER_API_WITH_CAPS(bmlFree, BML_API_Free, BML_CAP_MEMORY_BASIC);

        /* Aligned allocation */
        BML_REGISTER_API_WITH_CAPS(bmlAllocAligned, BML_API_AllocAligned, BML_CAP_MEMORY_ALIGNED);
        BML_REGISTER_API_WITH_CAPS(bmlFreeAligned, BML_API_FreeAligned, BML_CAP_MEMORY_ALIGNED);

        /* Memory pools - guarded for error handling */
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlMemoryPoolCreate, "memory.pool", BML_API_MemoryPoolCreate, BML_CAP_MEMORY_POOL);
        BML_REGISTER_API_WITH_CAPS(bmlMemoryPoolAlloc, BML_API_MemoryPoolAlloc, BML_CAP_MEMORY_POOL);
        BML_REGISTER_API_WITH_CAPS(bmlMemoryPoolFree, BML_API_MemoryPoolFree, BML_CAP_MEMORY_POOL);
        BML_REGISTER_API_WITH_CAPS(bmlMemoryPoolDestroy, BML_API_MemoryPoolDestroy, BML_CAP_MEMORY_POOL);

        /* Statistics and capabilities */
        BML_REGISTER_CAPS_API_WITH_CAPS(bmlGetMemoryStats, "memory.stats", BML_API_GetMemoryStats, BML_CAP_MEMORY_BASIC);
        BML_REGISTER_CAPS_API_WITH_CAPS(bmlGetMemoryCaps, "memory.caps", BML_API_GetMemoryCaps, BML_CAP_MEMORY_BASIC);
    }
} // namespace BML::Core
