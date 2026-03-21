#include "ApiRegistrationMacros.h"
#include "MemoryManager.h"

#include "bml_memory.h"

namespace BML::Core {
    void *BML_API_Alloc(size_t size) {
        return GetKernelOrNull()->memory->Alloc(size);
    }

    void *BML_API_Calloc(size_t count, size_t size) {
        return GetKernelOrNull()->memory->Calloc(count, size);
    }

    void *BML_API_Realloc(void *ptr, size_t old_size, size_t new_size) {
        return GetKernelOrNull()->memory->Realloc(ptr, old_size, new_size);
    }

    void BML_API_Free(void *ptr) {
        GetKernelOrNull()->memory->Free(ptr);
    }

    void BML_API_FreeWithSize(void *ptr, size_t size) {
        GetKernelOrNull()->memory->FreeWithSize(ptr, size);
    }

    void *BML_API_AllocAligned(size_t size, size_t alignment) {
        return GetKernelOrNull()->memory->AllocAligned(size, alignment);
    }

    void BML_API_FreeAligned(void *ptr) {
        GetKernelOrNull()->memory->FreeAligned(ptr);
    }

    BML_Result BML_API_MemoryPoolCreate(size_t block_size, uint32_t initial_blocks, BML_MemoryPool *out_pool) {
        return GetKernelOrNull()->memory->CreatePool(block_size, initial_blocks, out_pool);
    }

    void *BML_API_MemoryPoolAlloc(BML_MemoryPool pool) {
        return GetKernelOrNull()->memory->PoolAlloc(pool);
    }

    void BML_API_MemoryPoolFree(BML_MemoryPool pool, void *ptr) {
        GetKernelOrNull()->memory->PoolFree(pool, ptr);
    }

    void BML_API_MemoryPoolDestroy(BML_MemoryPool pool) {
        GetKernelOrNull()->memory->DestroyPool(pool);
    }

    BML_Result BML_API_GetMemoryStats(BML_MemoryStats *out_stats) {
        return GetKernelOrNull()->memory->GetStats(out_stats);
    }

    void RegisterMemoryApis() {
        BML_BEGIN_API_REGISTRATION();

        /* Basic allocation - direct functions, no error guard */
        BML_REGISTER_API(bmlAlloc, BML_API_Alloc);
        BML_REGISTER_API(bmlCalloc, BML_API_Calloc);
        BML_REGISTER_API(bmlRealloc, BML_API_Realloc);
        BML_REGISTER_API(bmlFree, BML_API_Free);

        /* Aligned allocation */
        BML_REGISTER_API(bmlAllocAligned, BML_API_AllocAligned);
        BML_REGISTER_API(bmlFreeAligned, BML_API_FreeAligned);

        /* Memory pools - guarded for error handling */
        BML_REGISTER_API_GUARDED(bmlMemoryPoolCreate, "memory.pool", BML_API_MemoryPoolCreate);
        BML_REGISTER_API(bmlMemoryPoolAlloc, BML_API_MemoryPoolAlloc);
        BML_REGISTER_API(bmlMemoryPoolFree, BML_API_MemoryPoolFree);
        BML_REGISTER_API(bmlMemoryPoolDestroy, BML_API_MemoryPoolDestroy);

        /* Statistics */
        BML_REGISTER_API_GUARDED(bmlGetMemoryStats, "memory.stats", BML_API_GetMemoryStats);
    }
} // namespace BML::Core
