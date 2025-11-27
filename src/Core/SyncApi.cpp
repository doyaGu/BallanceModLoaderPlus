#include "ApiRegistrationMacros.h"
#include "SyncManager.h"
#include "bml_capabilities.h"

#include "bml_sync.h"

namespace BML::Core {
    // Mutex
    BML_Result BML_API_MutexCreate(BML_Mutex *out_mutex) {
        return SyncManager::Instance().CreateMutex(out_mutex);
    }

    void BML_API_MutexDestroy(BML_Mutex mutex) {
        SyncManager::Instance().DestroyMutex(mutex);
    }

    void BML_API_MutexLock(BML_Mutex mutex) {
        SyncManager::Instance().LockMutex(mutex);
    }

    BML_Bool BML_API_MutexTryLock(BML_Mutex mutex) {
        return SyncManager::Instance().TryLockMutex(mutex);
    }

    void BML_API_MutexUnlock(BML_Mutex mutex) {
        SyncManager::Instance().UnlockMutex(mutex);
    }

    // RwLock
    BML_Result BML_API_RwLockCreate(BML_RwLock *out_lock) {
        return SyncManager::Instance().CreateRwLock(out_lock);
    }

    void BML_API_RwLockDestroy(BML_RwLock lock) {
        SyncManager::Instance().DestroyRwLock(lock);
    }

    void BML_API_RwLockReadLock(BML_RwLock lock) {
        SyncManager::Instance().ReadLockRwLock(lock);
    }

    BML_Bool BML_API_RwLockTryReadLock(BML_RwLock lock) {
        return SyncManager::Instance().TryReadLockRwLock(lock);
    }

    void BML_API_RwLockWriteLock(BML_RwLock lock) {
        SyncManager::Instance().WriteLockRwLock(lock);
    }

    BML_Bool BML_API_RwLockTryWriteLock(BML_RwLock lock) {
        return SyncManager::Instance().TryWriteLockRwLock(lock);
    }

    void BML_API_RwLockUnlock(BML_RwLock lock) {
        SyncManager::Instance().UnlockRwLock(lock);
    }

    void BML_API_RwLockReadUnlock(BML_RwLock lock) {
        SyncManager::Instance().ReadUnlockRwLock(lock);
    }

    void BML_API_RwLockWriteUnlock(BML_RwLock lock) {
        SyncManager::Instance().WriteUnlockRwLock(lock);
    }

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

    // Semaphore
    BML_Result BML_API_SemaphoreCreate(uint32_t initial_count, uint32_t max_count, BML_Semaphore *out_semaphore) {
        return SyncManager::Instance().CreateSemaphore(initial_count, max_count, out_semaphore);
    }

    void BML_API_SemaphoreDestroy(BML_Semaphore semaphore) {
        SyncManager::Instance().DestroySemaphore(semaphore);
    }

    BML_Result BML_API_SemaphoreWait(BML_Semaphore semaphore, uint32_t timeout_ms) {
        return SyncManager::Instance().WaitSemaphore(semaphore, timeout_ms);
    }

    BML_Result BML_API_SemaphoreSignal(BML_Semaphore semaphore, uint32_t count) {
        return SyncManager::Instance().SignalSemaphore(semaphore, count);
    }

    // TLS
    BML_Result BML_API_TlsCreate(BML_TlsDestructor destructor, BML_TlsKey *out_key) {
        return SyncManager::Instance().CreateTls(destructor, out_key);
    }

    void BML_API_TlsDestroy(BML_TlsKey key) {
        SyncManager::Instance().DestroyTls(key);
    }

    void *BML_API_TlsGet(BML_TlsKey key) {
        return SyncManager::Instance().GetTls(key);
    }

    BML_Result BML_API_TlsSet(BML_TlsKey key, void *value) {
        return SyncManager::Instance().SetTls(key, value);
    }

    // CondVar
    BML_Result BML_API_CondVarCreate(BML_CondVar *out_condvar) {
        return SyncManager::Instance().CreateCondVar(out_condvar);
    }

    void BML_API_CondVarDestroy(BML_CondVar condvar) {
        SyncManager::Instance().DestroyCondVar(condvar);
    }

    BML_Result BML_API_CondVarWait(BML_CondVar condvar, BML_Mutex mutex) {
        return SyncManager::Instance().WaitCondVar(condvar, mutex);
    }

    BML_Result BML_API_CondVarWaitTimeout(BML_CondVar condvar, BML_Mutex mutex, uint32_t timeout_ms) {
        return SyncManager::Instance().WaitCondVarTimeout(condvar, mutex, timeout_ms);
    }

    BML_Result BML_API_CondVarSignal(BML_CondVar condvar) {
        return SyncManager::Instance().SignalCondVar(condvar);
    }

    BML_Result BML_API_CondVarBroadcast(BML_CondVar condvar) {
        return SyncManager::Instance().BroadcastCondVar(condvar);
    }

    // SpinLock
    BML_Result BML_API_SpinLockCreate(BML_SpinLock *out_lock) {
        return SyncManager::Instance().CreateSpinLock(out_lock);
    }

    void BML_API_SpinLockDestroy(BML_SpinLock lock) {
        SyncManager::Instance().DestroySpinLock(lock);
    }

    void BML_API_SpinLockLock(BML_SpinLock lock) {
        SyncManager::Instance().LockSpinLock(lock);
    }

    BML_Bool BML_API_SpinLockTryLock(BML_SpinLock lock) {
        return SyncManager::Instance().TryLockSpinLock(lock);
    }

    void BML_API_SpinLockUnlock(BML_SpinLock lock) {
        SyncManager::Instance().UnlockSpinLock(lock);
    }

    // Capabilities
    BML_Result BML_API_GetSyncCaps(BML_SyncCaps *out_caps) {
        return SyncManager::Instance().GetCaps(out_caps);
    }

    void RegisterSyncApis() {
        BML_BEGIN_API_REGISTRATION();

        /* Mutex APIs */
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlMutexCreate, "sync.mutex", BML_API_MutexCreate, BML_CAP_SYNC_MUTEX);
        BML_REGISTER_API_WITH_CAPS(bmlMutexDestroy, BML_API_MutexDestroy, BML_CAP_SYNC_MUTEX);
        BML_REGISTER_API_WITH_CAPS(bmlMutexLock, BML_API_MutexLock, BML_CAP_SYNC_MUTEX);
        BML_REGISTER_API_WITH_CAPS(bmlMutexTryLock, BML_API_MutexTryLock, BML_CAP_SYNC_MUTEX);
        BML_REGISTER_API_WITH_CAPS(bmlMutexUnlock, BML_API_MutexUnlock, BML_CAP_SYNC_MUTEX);

        /* RwLock APIs */
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlRwLockCreate, "sync.rwlock", BML_API_RwLockCreate, BML_CAP_SYNC_RWLOCK);
        BML_REGISTER_API_WITH_CAPS(bmlRwLockDestroy, BML_API_RwLockDestroy, BML_CAP_SYNC_RWLOCK);
        BML_REGISTER_API_WITH_CAPS(bmlRwLockReadLock, BML_API_RwLockReadLock, BML_CAP_SYNC_RWLOCK);
        BML_REGISTER_API_WITH_CAPS(bmlRwLockTryReadLock, BML_API_RwLockTryReadLock, BML_CAP_SYNC_RWLOCK);
        BML_REGISTER_API_WITH_CAPS(bmlRwLockWriteLock, BML_API_RwLockWriteLock, BML_CAP_SYNC_RWLOCK);
        BML_REGISTER_API_WITH_CAPS(bmlRwLockTryWriteLock, BML_API_RwLockTryWriteLock, BML_CAP_SYNC_RWLOCK);
        BML_REGISTER_API_WITH_CAPS(bmlRwLockUnlock, BML_API_RwLockUnlock, BML_CAP_SYNC_RWLOCK);
        BML_REGISTER_API_WITH_CAPS(bmlRwLockReadUnlock, BML_API_RwLockReadUnlock, BML_CAP_SYNC_RWLOCK);
        BML_REGISTER_API_WITH_CAPS(bmlRwLockWriteUnlock, BML_API_RwLockWriteUnlock, BML_CAP_SYNC_RWLOCK);

        /* Atomic APIs - direct, no guard needed */
        BML_REGISTER_API_WITH_CAPS(bmlAtomicIncrement32, BML_API_AtomicIncrement32, BML_CAP_SYNC_ATOMIC);
        BML_REGISTER_API_WITH_CAPS(bmlAtomicDecrement32, BML_API_AtomicDecrement32, BML_CAP_SYNC_ATOMIC);
        BML_REGISTER_API_WITH_CAPS(bmlAtomicAdd32, BML_API_AtomicAdd32, BML_CAP_SYNC_ATOMIC);
        BML_REGISTER_API_WITH_CAPS(bmlAtomicCompareExchange32, BML_API_AtomicCompareExchange32, BML_CAP_SYNC_ATOMIC);
        BML_REGISTER_API_WITH_CAPS(bmlAtomicExchange32, BML_API_AtomicExchange32, BML_CAP_SYNC_ATOMIC);
        BML_REGISTER_API_WITH_CAPS(bmlAtomicLoadPtr, BML_API_AtomicLoadPtr, BML_CAP_SYNC_ATOMIC);
        BML_REGISTER_API_WITH_CAPS(bmlAtomicStorePtr, BML_API_AtomicStorePtr, BML_CAP_SYNC_ATOMIC);
        BML_REGISTER_API_WITH_CAPS(bmlAtomicCompareExchangePtr, BML_API_AtomicCompareExchangePtr, BML_CAP_SYNC_ATOMIC);

        /* Semaphore APIs */
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlSemaphoreCreate, "sync.semaphore", BML_API_SemaphoreCreate, BML_CAP_SYNC_SEMAPHORE);
        BML_REGISTER_API_WITH_CAPS(bmlSemaphoreDestroy, BML_API_SemaphoreDestroy, BML_CAP_SYNC_SEMAPHORE);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlSemaphoreWait, "sync.semaphore", BML_API_SemaphoreWait, BML_CAP_SYNC_SEMAPHORE);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlSemaphoreSignal, "sync.semaphore", BML_API_SemaphoreSignal, BML_CAP_SYNC_SEMAPHORE);

        /* TLS APIs */
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlTlsCreate, "sync.tls", BML_API_TlsCreate, BML_CAP_SYNC_TLS);
        BML_REGISTER_API_WITH_CAPS(bmlTlsDestroy, BML_API_TlsDestroy, BML_CAP_SYNC_TLS);
        BML_REGISTER_API_WITH_CAPS(bmlTlsGet, BML_API_TlsGet, BML_CAP_SYNC_TLS);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlTlsSet, "sync.tls", BML_API_TlsSet, BML_CAP_SYNC_TLS);

        /* CondVar APIs */
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlCondVarCreate, "sync.condvar", BML_API_CondVarCreate, BML_CAP_SYNC_MUTEX);
        BML_REGISTER_API_WITH_CAPS(bmlCondVarDestroy, BML_API_CondVarDestroy, BML_CAP_SYNC_MUTEX);
        BML_REGISTER_API_WITH_CAPS(bmlCondVarWait, BML_API_CondVarWait, BML_CAP_SYNC_MUTEX);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlCondVarWaitTimeout, "sync.condvar", BML_API_CondVarWaitTimeout, BML_CAP_SYNC_MUTEX);
        BML_REGISTER_API_WITH_CAPS(bmlCondVarSignal, BML_API_CondVarSignal, BML_CAP_SYNC_MUTEX);
        BML_REGISTER_API_WITH_CAPS(bmlCondVarBroadcast, BML_API_CondVarBroadcast, BML_CAP_SYNC_MUTEX);

        /* SpinLock APIs */
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlSpinLockCreate, "sync.spinlock", BML_API_SpinLockCreate, BML_CAP_SYNC_MUTEX);
        BML_REGISTER_API_WITH_CAPS(bmlSpinLockDestroy, BML_API_SpinLockDestroy, BML_CAP_SYNC_MUTEX);
        BML_REGISTER_API_WITH_CAPS(bmlSpinLockLock, BML_API_SpinLockLock, BML_CAP_SYNC_MUTEX);
        BML_REGISTER_API_WITH_CAPS(bmlSpinLockTryLock, BML_API_SpinLockTryLock, BML_CAP_SYNC_MUTEX);
        BML_REGISTER_API_WITH_CAPS(bmlSpinLockUnlock, BML_API_SpinLockUnlock, BML_CAP_SYNC_MUTEX);

        /* Capabilities */
        BML_REGISTER_CAPS_API_WITH_CAPS(bmlGetSyncCaps, "sync.caps", BML_API_GetSyncCaps,
                                        BML_CAP_SYNC_MUTEX | BML_CAP_SYNC_RWLOCK | BML_CAP_SYNC_SEMAPHORE | BML_CAP_SYNC_ATOMIC | BML_CAP_SYNC_TLS);
    }
} // namespace BML::Core
