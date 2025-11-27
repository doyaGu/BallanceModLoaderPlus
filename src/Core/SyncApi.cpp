#include "ApiRegistrationMacros.h"
#include "SyncManager.h"

#include "bml_sync.h"

namespace BML::Core {

// Forward declare C API functions
extern "C" {
    // Mutex
    BML_Result BML_API_MutexCreate(BML_Mutex* out_mutex);
    void BML_API_MutexDestroy(BML_Mutex mutex);
    void BML_API_MutexLock(BML_Mutex mutex);
    BML_Bool BML_API_MutexTryLock(BML_Mutex mutex);
    void BML_API_MutexUnlock(BML_Mutex mutex);
    
    // RwLock
    BML_Result BML_API_RwLockCreate(BML_RwLock* out_lock);
    void BML_API_RwLockDestroy(BML_RwLock lock);
    void BML_API_RwLockReadLock(BML_RwLock lock);
    BML_Bool BML_API_RwLockTryReadLock(BML_RwLock lock);
    void BML_API_RwLockWriteLock(BML_RwLock lock);
    BML_Bool BML_API_RwLockTryWriteLock(BML_RwLock lock);
    void BML_API_RwLockUnlock(BML_RwLock lock);
    void BML_API_RwLockReadUnlock(BML_RwLock lock);
    void BML_API_RwLockWriteUnlock(BML_RwLock lock);
    
    // Atomics
    int32_t BML_API_AtomicIncrement32(volatile int32_t* value);
    int32_t BML_API_AtomicDecrement32(volatile int32_t* value);
    int32_t BML_API_AtomicAdd32(volatile int32_t* value, int32_t addend);
    int32_t BML_API_AtomicCompareExchange32(volatile int32_t* dest, int32_t exchange, int32_t comparand);
    int32_t BML_API_AtomicExchange32(volatile int32_t* dest, int32_t new_value);
    void* BML_API_AtomicLoadPtr(void* volatile* ptr);
    void BML_API_AtomicStorePtr(void* volatile* ptr, void* value);
    void* BML_API_AtomicCompareExchangePtr(void* volatile* dest, void* exchange, void* comparand);
    
    // Semaphore
    BML_Result BML_API_SemaphoreCreate(uint32_t initial_count, uint32_t max_count, BML_Semaphore* out_semaphore);
    void BML_API_SemaphoreDestroy(BML_Semaphore semaphore);
    BML_Result BML_API_SemaphoreWait(BML_Semaphore semaphore, uint32_t timeout_ms);
    BML_Result BML_API_SemaphoreSignal(BML_Semaphore semaphore, uint32_t count);
    
    // TLS
    BML_Result BML_API_TlsCreate(BML_TlsDestructor destructor, BML_TlsKey* out_key);
    void BML_API_TlsDestroy(BML_TlsKey key);
    void* BML_API_TlsGet(BML_TlsKey key);
    BML_Result BML_API_TlsSet(BML_TlsKey key, void* value);
    
    // CondVar
    BML_Result BML_API_CondVarCreate(BML_CondVar* out_condvar);
    void BML_API_CondVarDestroy(BML_CondVar condvar);
    void BML_API_CondVarWait(BML_CondVar condvar, BML_Mutex mutex);
    BML_Result BML_API_CondVarWaitTimeout(BML_CondVar condvar, BML_Mutex mutex, uint32_t timeout_ms);
    void BML_API_CondVarSignal(BML_CondVar condvar);
    void BML_API_CondVarBroadcast(BML_CondVar condvar);
    
    // SpinLock
    BML_Result BML_API_SpinLockCreate(BML_SpinLock* out_lock);
    void BML_API_SpinLockDestroy(BML_SpinLock lock);
    void BML_API_SpinLockLock(BML_SpinLock lock);
    BML_Bool BML_API_SpinLockTryLock(BML_SpinLock lock);
    void BML_API_SpinLockUnlock(BML_SpinLock lock);
    
    // Capabilities
    BML_Result BML_API_GetSyncCaps(BML_SyncCaps* out_caps);
}

void RegisterSyncApis() {
    BML_BEGIN_API_REGISTRATION();
    
    /* Mutex APIs */
    BML_REGISTER_API_GUARDED(bmlMutexCreate, "sync.mutex", BML_API_MutexCreate);
    BML_REGISTER_API(bmlMutexDestroy, BML_API_MutexDestroy);
    BML_REGISTER_API(bmlMutexLock, BML_API_MutexLock);
    BML_REGISTER_API(bmlMutexTryLock, BML_API_MutexTryLock);
    BML_REGISTER_API(bmlMutexUnlock, BML_API_MutexUnlock);
    
    /* RwLock APIs */
    BML_REGISTER_API_GUARDED(bmlRwLockCreate, "sync.rwlock", BML_API_RwLockCreate);
    BML_REGISTER_API(bmlRwLockDestroy, BML_API_RwLockDestroy);
    BML_REGISTER_API(bmlRwLockReadLock, BML_API_RwLockReadLock);
    BML_REGISTER_API(bmlRwLockTryReadLock, BML_API_RwLockTryReadLock);
    BML_REGISTER_API(bmlRwLockWriteLock, BML_API_RwLockWriteLock);
    BML_REGISTER_API(bmlRwLockTryWriteLock, BML_API_RwLockTryWriteLock);
    BML_REGISTER_API(bmlRwLockUnlock, BML_API_RwLockUnlock);
    BML_REGISTER_API(bmlRwLockReadUnlock, BML_API_RwLockReadUnlock);
    BML_REGISTER_API(bmlRwLockWriteUnlock, BML_API_RwLockWriteUnlock);

    /* Atomic APIs - direct, no guard needed */
    BML_REGISTER_API(bmlAtomicIncrement32, BML_API_AtomicIncrement32);
    BML_REGISTER_API(bmlAtomicDecrement32, BML_API_AtomicDecrement32);
    BML_REGISTER_API(bmlAtomicAdd32, BML_API_AtomicAdd32);
    BML_REGISTER_API(bmlAtomicCompareExchange32, BML_API_AtomicCompareExchange32);
    BML_REGISTER_API(bmlAtomicExchange32, BML_API_AtomicExchange32);
    BML_REGISTER_API(bmlAtomicLoadPtr, BML_API_AtomicLoadPtr);
    BML_REGISTER_API(bmlAtomicStorePtr, BML_API_AtomicStorePtr);
    BML_REGISTER_API(bmlAtomicCompareExchangePtr, BML_API_AtomicCompareExchangePtr);
    
    /* Semaphore APIs */
    BML_REGISTER_API_GUARDED(bmlSemaphoreCreate, "sync.semaphore", BML_API_SemaphoreCreate);
    BML_REGISTER_API(bmlSemaphoreDestroy, BML_API_SemaphoreDestroy);
    BML_REGISTER_API_GUARDED(bmlSemaphoreWait, "sync.semaphore", BML_API_SemaphoreWait);
    BML_REGISTER_API_GUARDED(bmlSemaphoreSignal, "sync.semaphore", BML_API_SemaphoreSignal);
    
    /* TLS APIs */
    BML_REGISTER_API_GUARDED(bmlTlsCreate, "sync.tls", BML_API_TlsCreate);
    BML_REGISTER_API(bmlTlsDestroy, BML_API_TlsDestroy);
    BML_REGISTER_API(bmlTlsGet, BML_API_TlsGet);
    BML_REGISTER_API_GUARDED(bmlTlsSet, "sync.tls", BML_API_TlsSet);
    
    /* CondVar APIs */
    BML_REGISTER_API_GUARDED(bmlCondVarCreate, "sync.condvar", BML_API_CondVarCreate);
    BML_REGISTER_API(bmlCondVarDestroy, BML_API_CondVarDestroy);
    BML_REGISTER_API(bmlCondVarWait, BML_API_CondVarWait);
    BML_REGISTER_API_GUARDED(bmlCondVarWaitTimeout, "sync.condvar", BML_API_CondVarWaitTimeout);
    BML_REGISTER_API(bmlCondVarSignal, BML_API_CondVarSignal);
    BML_REGISTER_API(bmlCondVarBroadcast, BML_API_CondVarBroadcast);
    
    /* SpinLock APIs */
    BML_REGISTER_API_GUARDED(bmlSpinLockCreate, "sync.spinlock", BML_API_SpinLockCreate);
    BML_REGISTER_API(bmlSpinLockDestroy, BML_API_SpinLockDestroy);
    BML_REGISTER_API(bmlSpinLockLock, BML_API_SpinLockLock);
    BML_REGISTER_API(bmlSpinLockTryLock, BML_API_SpinLockTryLock);
    BML_REGISTER_API(bmlSpinLockUnlock, BML_API_SpinLockUnlock);
    
    /* Capabilities */
    BML_REGISTER_CAPS_API(bmlGetSyncCaps, "sync.caps", BML_API_GetSyncCaps);
}

} // namespace BML::Core
