#include "ApiRegistrationMacros.h"
#include "SyncManager.h"

#include "bml_sync.h"

namespace BML::Core {
    // Atomics
    int32_t BML_API_AtomicIncrement32(volatile int32_t *value) {
        return SyncManager::AtomicIncrement32(value);
    }

    int32_t BML_API_AtomicDecrement32(volatile int32_t *value) {
        return SyncManager::AtomicDecrement32(value);
    }

    int32_t BML_API_AtomicAdd32(volatile int32_t *value, int32_t addend) {
        return SyncManager::AtomicAdd32(value, addend);
    }

    int32_t BML_API_AtomicCompareExchange32(volatile int32_t *dest, int32_t exchange, int32_t comparand) {
        return SyncManager::AtomicCompareExchange32(dest, exchange, comparand);
    }

    int32_t BML_API_AtomicExchange32(volatile int32_t *dest, int32_t new_value) {
        return SyncManager::AtomicExchange32(dest, new_value);
    }

    void *BML_API_AtomicLoadPtr(void *volatile*ptr) {
        return SyncManager::AtomicLoadPtr(ptr);
    }

    void BML_API_AtomicStorePtr(void *volatile*ptr, void *value) {
        SyncManager::AtomicStorePtr(ptr, value);
    }

    void *BML_API_AtomicCompareExchangePtr(void *volatile*dest, void *exchange, void *comparand) {
        return SyncManager::AtomicCompareExchangePtr(dest, exchange, comparand);
    }

    void RegisterSyncApis(ApiRegistry &apiRegistry) {
        BML_BEGIN_API_REGISTRATION(apiRegistry);

        /* Atomic APIs - direct, no guard needed */
        BML_REGISTER_API(bmlAtomicIncrement32, BML_API_AtomicIncrement32);
        BML_REGISTER_API(bmlAtomicDecrement32, BML_API_AtomicDecrement32);
        BML_REGISTER_API(bmlAtomicAdd32, BML_API_AtomicAdd32);
        BML_REGISTER_API(bmlAtomicCompareExchange32, BML_API_AtomicCompareExchange32);
        BML_REGISTER_API(bmlAtomicExchange32, BML_API_AtomicExchange32);
        BML_REGISTER_API(bmlAtomicLoadPtr, BML_API_AtomicLoadPtr);
        BML_REGISTER_API(bmlAtomicStorePtr, BML_API_AtomicStorePtr);
        BML_REGISTER_API(bmlAtomicCompareExchangePtr, BML_API_AtomicCompareExchangePtr);
    }
} // namespace BML::Core
