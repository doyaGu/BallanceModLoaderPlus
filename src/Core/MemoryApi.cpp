#include "ApiRegistrationMacros.h"
#include "MemoryManager.h"
#include "bml_capabilities.h"

#include "bml_memory.h"

// Forward declare C API functions
extern "C" {
    void *BML_API_Alloc(size_t size);
    void *BML_API_Calloc(size_t count, size_t size);
    void *BML_API_Realloc(void *ptr, size_t new_size);
    void BML_API_Free(void *ptr);
    void *BML_API_AllocAligned(size_t size, size_t alignment);
    void BML_API_FreeAligned(void *ptr);
    BML_Result BML_API_MemoryPoolCreate(size_t block_size, uint32_t initial_blocks, BML_MemoryPool *out_pool);
    void *BML_API_MemoryPoolAlloc(BML_MemoryPool pool);
    void BML_API_MemoryPoolFree(BML_MemoryPool pool, void *ptr);
    void BML_API_MemoryPoolDestroy(BML_MemoryPool pool);
    BML_Result BML_API_GetMemoryStats(BML_MemoryStats *out_stats);
    BML_Result BML_API_GetMemoryCaps(BML_MemoryCaps *out_caps);
}

namespace BML::Core {

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
