#include "ApiRegistrationMacros.h"
#include "SyncManager.h"

#include "bml_sync.h"

namespace BML::Core {
    // Mutex
    BML_Result BML_API_MutexCreate(BML_Mutex *out_mutex) {
        return GetKernelOrNull()->sync->CreateMutex(out_mutex);
    }

    void BML_API_MutexDestroy(BML_Mutex mutex) {
        GetKernelOrNull()->sync->DestroyMutex(mutex);
    }

    void BML_API_MutexLock(BML_Mutex mutex) {
        GetKernelOrNull()->sync->LockMutex(mutex);
    }

    BML_Bool BML_API_MutexTryLock(BML_Mutex mutex) {
        return GetKernelOrNull()->sync->TryLockMutex(mutex);
    }

    void BML_API_MutexUnlock(BML_Mutex mutex) {
        GetKernelOrNull()->sync->UnlockMutex(mutex);
    }

    BML_Result BML_API_MutexLockTimeout(BML_Mutex mutex, uint32_t timeout_ms) {
        return GetKernelOrNull()->sync->LockMutexTimeout(mutex, timeout_ms);
    }

    // RwLock
    BML_Result BML_API_RwLockCreate(BML_RwLock *out_lock) {
        return GetKernelOrNull()->sync->CreateRwLock(out_lock);
    }

    void BML_API_RwLockDestroy(BML_RwLock lock) {
        GetKernelOrNull()->sync->DestroyRwLock(lock);
    }

    void BML_API_RwLockReadLock(BML_RwLock lock) {
        GetKernelOrNull()->sync->ReadLockRwLock(lock);
    }

    BML_Bool BML_API_RwLockTryReadLock(BML_RwLock lock) {
        return GetKernelOrNull()->sync->TryReadLockRwLock(lock);
    }

    BML_Result BML_API_RwLockReadLockTimeout(BML_RwLock lock, uint32_t timeout_ms) {
        return GetKernelOrNull()->sync->ReadLockRwLockTimeout(lock, timeout_ms);
    }

    void BML_API_RwLockWriteLock(BML_RwLock lock) {
        GetKernelOrNull()->sync->WriteLockRwLock(lock);
    }

    BML_Bool BML_API_RwLockTryWriteLock(BML_RwLock lock) {
        return GetKernelOrNull()->sync->TryWriteLockRwLock(lock);
    }

    BML_Result BML_API_RwLockWriteLockTimeout(BML_RwLock lock, uint32_t timeout_ms) {
        return GetKernelOrNull()->sync->WriteLockRwLockTimeout(lock, timeout_ms);
    }

    void BML_API_RwLockUnlock(BML_RwLock lock) {
        GetKernelOrNull()->sync->UnlockRwLock(lock);
    }

    void BML_API_RwLockReadUnlock(BML_RwLock lock) {
        GetKernelOrNull()->sync->ReadUnlockRwLock(lock);
    }

    void BML_API_RwLockWriteUnlock(BML_RwLock lock) {
        GetKernelOrNull()->sync->WriteUnlockRwLock(lock);
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
        return GetKernelOrNull()->sync->CreateSemaphore(initial_count, max_count, out_semaphore);
    }

    void BML_API_SemaphoreDestroy(BML_Semaphore semaphore) {
        GetKernelOrNull()->sync->DestroySemaphore(semaphore);
    }

    BML_Result BML_API_SemaphoreWait(BML_Semaphore semaphore, uint32_t timeout_ms) {
        return GetKernelOrNull()->sync->WaitSemaphore(semaphore, timeout_ms);
    }

    BML_Result BML_API_SemaphoreSignal(BML_Semaphore semaphore, uint32_t count) {
        return GetKernelOrNull()->sync->SignalSemaphore(semaphore, count);
    }

    // TLS
    BML_Result BML_API_TlsCreate(BML_TlsDestructor destructor, BML_TlsKey *out_key) {
        return GetKernelOrNull()->sync->CreateTls(destructor, out_key);
    }

    void BML_API_TlsDestroy(BML_TlsKey key) {
        GetKernelOrNull()->sync->DestroyTls(key);
    }

    void *BML_API_TlsGet(BML_TlsKey key) {
        return GetKernelOrNull()->sync->GetTls(key);
    }

    BML_Result BML_API_TlsSet(BML_TlsKey key, void *value) {
        return GetKernelOrNull()->sync->SetTls(key, value);
    }

    // CondVar
    BML_Result BML_API_CondVarCreate(BML_CondVar *out_condvar) {
        return GetKernelOrNull()->sync->CreateCondVar(out_condvar);
    }

    void BML_API_CondVarDestroy(BML_CondVar condvar) {
        GetKernelOrNull()->sync->DestroyCondVar(condvar);
    }

    BML_Result BML_API_CondVarWait(BML_CondVar condvar, BML_Mutex mutex) {
        return GetKernelOrNull()->sync->WaitCondVar(condvar, mutex);
    }

    BML_Result BML_API_CondVarWaitTimeout(BML_CondVar condvar, BML_Mutex mutex, uint32_t timeout_ms) {
        return GetKernelOrNull()->sync->WaitCondVarTimeout(condvar, mutex, timeout_ms);
    }

    BML_Result BML_API_CondVarSignal(BML_CondVar condvar) {
        return GetKernelOrNull()->sync->SignalCondVar(condvar);
    }

    BML_Result BML_API_CondVarBroadcast(BML_CondVar condvar) {
        return GetKernelOrNull()->sync->BroadcastCondVar(condvar);
    }

    // SpinLock
    BML_Result BML_API_SpinLockCreate(BML_SpinLock *out_lock) {
        return GetKernelOrNull()->sync->CreateSpinLock(out_lock);
    }

    void BML_API_SpinLockDestroy(BML_SpinLock lock) {
        GetKernelOrNull()->sync->DestroySpinLock(lock);
    }

    void BML_API_SpinLockLock(BML_SpinLock lock) {
        GetKernelOrNull()->sync->LockSpinLock(lock);
    }

    BML_Bool BML_API_SpinLockTryLock(BML_SpinLock lock) {
        return GetKernelOrNull()->sync->TryLockSpinLock(lock);
    }

    void BML_API_SpinLockUnlock(BML_SpinLock lock) {
        GetKernelOrNull()->sync->UnlockSpinLock(lock);
    }

    void RegisterSyncApis() {
        BML_BEGIN_API_REGISTRATION();

        /* Mutex APIs */
        BML_REGISTER_API_GUARDED(bmlMutexCreate, "sync.mutex", BML_API_MutexCreate);
        BML_REGISTER_API(bmlMutexDestroy, BML_API_MutexDestroy);
        BML_REGISTER_API(bmlMutexLock, BML_API_MutexLock);
        BML_REGISTER_API(bmlMutexTryLock, BML_API_MutexTryLock);
        BML_REGISTER_API_GUARDED(bmlMutexLockTimeout, "sync.mutex", BML_API_MutexLockTimeout);
        BML_REGISTER_API(bmlMutexUnlock, BML_API_MutexUnlock);

        /* RwLock APIs */
        BML_REGISTER_API_GUARDED(bmlRwLockCreate, "sync.rwlock", BML_API_RwLockCreate);
        BML_REGISTER_API(bmlRwLockDestroy, BML_API_RwLockDestroy);
        BML_REGISTER_API(bmlRwLockReadLock, BML_API_RwLockReadLock);
        BML_REGISTER_API(bmlRwLockTryReadLock, BML_API_RwLockTryReadLock);
        BML_REGISTER_API_GUARDED(bmlRwLockReadLockTimeout, "sync.rwlock", BML_API_RwLockReadLockTimeout);
        BML_REGISTER_API(bmlRwLockWriteLock, BML_API_RwLockWriteLock);
        BML_REGISTER_API(bmlRwLockTryWriteLock, BML_API_RwLockTryWriteLock);
        BML_REGISTER_API_GUARDED(bmlRwLockWriteLockTimeout, "sync.rwlock", BML_API_RwLockWriteLockTimeout);
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
    }
} // namespace BML::Core
