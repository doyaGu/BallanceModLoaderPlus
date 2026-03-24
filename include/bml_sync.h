#ifndef BML_SYNC_H
#define BML_SYNC_H

/**
 * @file bml_sync.h
 * @brief Synchronization primitives for multi-threaded mods
 * 
 * Provides portable synchronization primitives that work across different
 * compiler/CRT combinations. All primitives are implemented using OS-native
 * APIs for maximum performance.
 * 
 * Thread Safety:
 * - All create/destroy functions are thread-safe
 * - All lock/unlock operations are thread-safe
 * - Destroying a primitive while in use is undefined behavior
 */

#include "bml_types.h"
#include "bml_errors.h"
#include "bml_interface.h"
#include "bml_version.h"

BML_BEGIN_CDECLS

#define BML_CORE_SYNC_INTERFACE_ID "bml.core.sync"

/* ========== Timeout Constants ========== */

/**
 * @brief No timeout (non-blocking operation fails immediately if cannot acquire)
 */
#define BML_TIMEOUT_NONE     0u

/**
 * @brief Infinite timeout (blocking operation)
 */
#define BML_TIMEOUT_INFINITE 0xFFFFFFFFu

/* ========== Type-Safe Handle Declarations ========== */

/**
 * @brief Opaque mutex handle
 * 
 * Mutexes provide exclusive access to shared resources.
 * Non-recursive (cannot be locked twice by the same thread).
 */
BML_DECLARE_HANDLE(BML_Mutex);

/**
 * @brief Opaque read-write lock handle
 * 
 * Allows multiple concurrent readers or one exclusive writer.
 */
BML_DECLARE_HANDLE(BML_RwLock);

/**
 * @brief Opaque semaphore handle
 */
BML_DECLARE_HANDLE(BML_Semaphore);

/**
 * @brief Opaque condition variable handle
 */
BML_DECLARE_HANDLE(BML_CondVar);

/**
 * @brief Opaque spin lock handle
 */
BML_DECLARE_HANDLE(BML_SpinLock);

/**
 * @brief Opaque TLS key handle
 */
BML_DECLARE_HANDLE(BML_TlsKey);

/* ========== Mutex API ========== */

/**
 * @brief Create a mutex
 * 
 * @param[out] out_mutex Receives mutex handle
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_OUT_OF_MEMORY if allocation failed
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_MutexCreate)(BML_Context ctx, BML_Mod owner, BML_Mutex *out_mutex);

/**
 * @brief Destroy a mutex
 * 
 * @param[in] mutex Mutex handle
 * 
 * @threadsafe No (ensure no thread holds the mutex)
 *
 * @warning Destroying a locked mutex is undefined behavior
 * @note Passing NULL handle is a safe no-op.
 */
typedef void (*PFN_BML_MutexDestroy)(BML_Context ctx, BML_Mutex mutex);

/**
 * @brief Lock a mutex (blocking)
 *
 * @param[in] mutex Mutex handle
 *
 * @threadsafe Yes
 *
 * @note Passing NULL handle is a safe no-op.
 * @note Blocks until mutex is acquired
 * @warning Locking the same mutex twice from same thread causes deadlock
 */
typedef void (*PFN_BML_MutexLock)(BML_Context ctx, BML_Mutex mutex);

/**
 * @brief Try to lock a mutex (non-blocking)
 * 
 * @param[in] mutex Mutex handle
 * @return BML_TRUE if lock acquired, BML_FALSE otherwise
 * 
 * @threadsafe Yes
 */
typedef BML_Bool (*PFN_BML_MutexTryLock)(BML_Context ctx, BML_Mutex mutex);

/**
 * @brief Lock a mutex with timeout
 * 
 * @param[in] mutex Mutex handle
 * @param[in] timeout_ms Timeout in milliseconds (BML_TIMEOUT_NONE for non-blocking,
 *                       BML_TIMEOUT_INFINITE for blocking)
 * @return BML_RESULT_OK if lock acquired
 * @return BML_RESULT_TIMEOUT if timeout expired
 * @return BML_RESULT_INVALID_HANDLE if mutex is invalid
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_MutexLockTimeout)(BML_Context ctx,
                                               BML_Mutex mutex,
                                               uint32_t timeout_ms);

/**
 * @brief Unlock a mutex
 *
 * @param[in] mutex Mutex handle
 *
 * @threadsafe Yes
 *
 * @note Passing NULL handle is a safe no-op.
 * @warning Must be called by the thread that locked the mutex
 */
typedef void (*PFN_BML_MutexUnlock)(BML_Context ctx, BML_Mutex mutex);

/* ========== Read-Write Lock API ========== */

/**
 * @brief Create a read-write lock
 * 
 * @param[out] out_lock Receives lock handle
 * @return BML_RESULT_OK on success
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_RwLockCreate)(BML_Context ctx, BML_Mod owner, BML_RwLock *out_lock);

/**
 * @brief Destroy a read-write lock
 * 
 * @param[in] lock Lock handle
 * 
 * @threadsafe No
 */
typedef void (*PFN_BML_RwLockDestroy)(BML_Context ctx, BML_RwLock lock);

/**
 * @brief Acquire read lock (shared access)
 * 
 * @param[in] lock Lock handle
 * 
 * @threadsafe Yes
 * 
 * @note Multiple readers can hold the lock simultaneously
 * @note Blocks if a writer holds the lock
 */
typedef void (*PFN_BML_RwLockReadLock)(BML_Context ctx, BML_RwLock lock);

/**
 * @brief Try to acquire read lock (non-blocking)
 * 
 * @param[in] lock Lock handle
 * @return BML_TRUE if lock acquired, BML_FALSE otherwise
 * 
 * @threadsafe Yes
 */
typedef BML_Bool (*PFN_BML_RwLockTryReadLock)(BML_Context ctx, BML_RwLock lock);

/**
 * @brief Acquire read lock with timeout
 * 
 * @param[in] lock Lock handle
 * @param[in] timeout_ms Timeout in milliseconds (BML_TIMEOUT_NONE for non-blocking,
 *                       BML_TIMEOUT_INFINITE for blocking)
 * @return BML_RESULT_OK if lock acquired
 * @return BML_RESULT_TIMEOUT if timeout expired
 * @return BML_RESULT_INVALID_HANDLE if lock is invalid
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_RwLockReadLockTimeout)(BML_Context ctx,
                                                    BML_RwLock lock,
                                                    uint32_t timeout_ms);

/**
 * @brief Acquire write lock (exclusive access)
 * 
 * @param[in] lock Lock handle
 * 
 * @threadsafe Yes
 * 
 * @note Blocks until all readers and writers release the lock
 */
typedef void (*PFN_BML_RwLockWriteLock)(BML_Context ctx, BML_RwLock lock);

/**
 * @brief Try to acquire write lock (non-blocking)
 * 
 * @param[in] lock Lock handle
 * @return BML_TRUE if lock acquired, BML_FALSE otherwise
 * 
 * @threadsafe Yes
 */
typedef BML_Bool (*PFN_BML_RwLockTryWriteLock)(BML_Context ctx, BML_RwLock lock);

/**
 * @brief Acquire write lock with timeout
 * 
 * @param[in] lock Lock handle
 * @param[in] timeout_ms Timeout in milliseconds (BML_TIMEOUT_NONE for non-blocking,
 *                       BML_TIMEOUT_INFINITE for blocking)
 * @return BML_RESULT_OK if lock acquired
 * @return BML_RESULT_TIMEOUT if timeout expired
 * @return BML_RESULT_INVALID_HANDLE if lock is invalid
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_RwLockWriteLockTimeout)(BML_Context ctx,
                                                     BML_RwLock lock,
                                                     uint32_t timeout_ms);

/**
 * @brief Release read or write lock (auto-detects lock type)
 *
 * @param[in] lock Lock handle
 *
 * @threadsafe Yes
 *
 * @note Uses internal tracking to determine lock type. For explicit control,
 *       prefer bmlRwLockReadUnlock or bmlRwLockWriteUnlock.
 */
typedef void (*PFN_BML_RwLockUnlock)(BML_Context ctx, BML_RwLock lock);

/**
 * @brief Explicitly release a read lock
 *
 * @param[in] lock Lock handle
 *
 * @threadsafe Yes
 *
 * @warning Only call this after acquiring via ReadLock/TryReadLock
 */
typedef void (*PFN_BML_RwLockReadUnlock)(BML_Context ctx, BML_RwLock lock);

/**
 * @brief Explicitly release a write lock
 *
 * @param[in] lock Lock handle
 *
 * @threadsafe Yes
 *
 * @warning Only call this after acquiring via WriteLock/TryWriteLock
 */
typedef void (*PFN_BML_RwLockWriteUnlock)(BML_Context ctx, BML_RwLock lock);

/* ========== Atomic Operations ========== */

/**
 * @brief Atomically increment a 32-bit integer
 * 
 * @param[in,out] value Pointer to integer
 * @return New value after increment
 * 
 * @threadsafe Yes
 */
typedef int32_t (*PFN_BML_AtomicIncrement32)(volatile int32_t *value);

/**
 * @brief Atomically decrement a 32-bit integer
 * 
 * @param[in,out] value Pointer to integer
 * @return New value after decrement
 * 
 * @threadsafe Yes
 */
typedef int32_t (*PFN_BML_AtomicDecrement32)(volatile int32_t *value);

/**
 * @brief Atomically add to a 32-bit integer
 * 
 * @param[in,out] value Pointer to integer
 * @param[in] addend Value to add
 * @return Previous value before addition
 * 
 * @threadsafe Yes
 */
typedef int32_t (*PFN_BML_AtomicAdd32)(volatile int32_t *value, int32_t addend);

/**
 * @brief Atomically compare and exchange
 * 
 * If *dest == comparand, sets *dest = exchange and returns comparand.
 * Otherwise, returns current *dest value.
 * 
 * @param[in,out] dest Pointer to destination
 * @param[in] exchange Value to set if comparison succeeds
 * @param[in] comparand Value to compare against
 * @return Previous value of *dest
 * 
 * @threadsafe Yes
 * 
 * @code
 * // Spin-lock implementation
 * volatile int32_t lock = 0;
 * while (bmlAtomicCompareExchange32(&lock, 1, 0) != 0) {
 *     // Spin
 * }
 * // Critical section
 * bmlAtomicExchange32(&lock, 0);
 * @endcode
 */
typedef int32_t (*PFN_BML_AtomicCompareExchange32)(
    volatile int32_t *dest,
    int32_t exchange,
    int32_t comparand
);

/**
 * @brief Atomically exchange values
 * 
 * Sets *dest = new_value and returns old value.
 * 
 * @param[in,out] dest Pointer to destination
 * @param[in] new_value New value to set
 * @return Previous value of *dest
 * 
 * @threadsafe Yes
 */
typedef int32_t (*PFN_BML_AtomicExchange32)(volatile int32_t *dest, int32_t new_value);

/**
 * @brief Atomically load a pointer
 * 
 * @param[in] ptr Pointer to pointer
 * @return Current pointer value
 * 
 * @threadsafe Yes
 */
typedef void *(*PFN_BML_AtomicLoadPtr)(void *volatile *ptr);

/**
 * @brief Atomically store a pointer
 * 
 * @param[out] ptr Pointer to pointer
 * @param[in] value New pointer value
 * 
 * @threadsafe Yes
 */
typedef void (*PFN_BML_AtomicStorePtr)(void *volatile *ptr, void *value);

/**
 * @brief Atomically compare and exchange pointers
 * 
 * @param[in,out] dest Pointer to destination pointer
 * @param[in] exchange Pointer to set if comparison succeeds
 * @param[in] comparand Pointer to compare against
 * @return Previous pointer value
 * 
 * @threadsafe Yes
 */
typedef void *(*PFN_BML_AtomicCompareExchangePtr)(
    void *volatile *dest,
    void *exchange,
    void *comparand
);

/* ========== Semaphore API ========== */

/**
 * @brief Create a semaphore
 * 
 * @param[in] initial_count Initial count value
 * @param[in] max_count Maximum count value
 * @param[out] out_semaphore Receives semaphore handle
 * @return BML_RESULT_OK on success
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_SemaphoreCreate)(
    BML_Context ctx,
    BML_Mod owner,
    uint32_t initial_count,
    uint32_t max_count,
    BML_Semaphore *out_semaphore
);

/**
 * @brief Destroy a semaphore
 * 
 * @param[in] semaphore Semaphore handle
 * 
 * @threadsafe No
 */
typedef void (*PFN_BML_SemaphoreDestroy)(BML_Context ctx, BML_Semaphore semaphore);

/**
 * @brief Wait on semaphore (decrement count)
 * 
 * @param[in] semaphore Semaphore handle
 * @param[in] timeout_ms Timeout in milliseconds (0xFFFFFFFF = infinite)
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_TIMEOUT if timeout expired
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_SemaphoreWait)(BML_Context ctx,
                                            BML_Semaphore semaphore,
                                            uint32_t timeout_ms);

/**
 * @brief Signal semaphore (increment count)
 * 
 * @param[in] semaphore Semaphore handle
 * @param[in] count Number to increment by
 * @return BML_RESULT_OK on success
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_SemaphoreSignal)(BML_Context ctx,
                                              BML_Semaphore semaphore,
                                              uint32_t count);

/* ========== Thread Local Storage API ========== */

/**
 * @brief Destructor callback for TLS values
 */
typedef void (*BML_TlsDestructor)(void *value);

/**
 * @brief Create a TLS key
 * 
 * @param[in] destructor Destructor called on thread exit (may be NULL)
 * @param[out] out_key Receives TLS key
 * @return BML_RESULT_OK on success
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_TlsCreate)(BML_Context ctx,
                                        BML_TlsDestructor destructor,
                                        BML_TlsKey *out_key);

/**
 * @brief Destroy a TLS key
 * 
 * @param[in] key TLS key
 * 
 * @threadsafe No
 */
typedef void (*PFN_BML_TlsDestroy)(BML_Context ctx, BML_TlsKey key);

/**
 * @brief Get thread-local value
 * 
 * @param[in] key TLS key
 * @return Thread-local value (NULL if not set)
 * 
 * @threadsafe Yes
 */
typedef void *(*PFN_BML_TlsGet)(BML_Context ctx, BML_TlsKey key);

/**
 * @brief Set thread-local value
 * 
 * @param[in] key TLS key
 * @param[in] value Value to set (may be NULL)
 * @return BML_RESULT_OK on success
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_TlsSet)(BML_Context ctx, BML_TlsKey key, void *value);


/* ========== Condition Variable API ========== */

/**
 * @brief Create a condition variable
 * 
 * @param[out] out_condvar Receives condition variable handle
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_OUT_OF_MEMORY if allocation failed
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_CondVarCreate)(BML_Context ctx, BML_Mod owner, BML_CondVar *out_condvar);

/**
 * @brief Destroy a condition variable
 * 
 * @param[in] condvar Condition variable handle
 * 
 * @threadsafe No (ensure no thread is waiting)
 */
typedef void (*PFN_BML_CondVarDestroy)(BML_Context ctx, BML_CondVar condvar);

/**
 * @brief Wait on a condition variable
 * 
 * Atomically releases the mutex and waits on the condition variable.
 * When signaled, re-acquires the mutex before returning.
 * 
 * @param[in] condvar Condition variable handle
 * @param[in] mutex Mutex handle (must be locked by caller)
 * @return BML_RESULT_OK when signaled
 * 
 * @threadsafe Yes
 * 
 * @warning Spurious wakeups may occur; always check the condition in a loop
 */
typedef BML_Result (*PFN_BML_CondVarWait)(BML_Context ctx, BML_CondVar condvar, BML_Mutex mutex);

/**
 * @brief Wait on a condition variable with timeout
 * 
 * @param[in] condvar Condition variable handle
 * @param[in] mutex Mutex handle (must be locked by caller)
 * @param[in] timeout_ms Timeout in milliseconds
 * @return BML_RESULT_OK when signaled
 * @return BML_RESULT_TIMEOUT if timeout expired
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_CondVarWaitTimeout)(BML_Context ctx,
                                                 BML_CondVar condvar,
                                                 BML_Mutex mutex,
                                                 uint32_t timeout_ms);

/**
 * @brief Signal one waiting thread
 * 
 * @param[in] condvar Condition variable handle
 * @return BML_RESULT_OK on success
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_CondVarSignal)(BML_Context ctx, BML_CondVar condvar);

/**
 * @brief Signal all waiting threads
 * 
 * @param[in] condvar Condition variable handle
 * @return BML_RESULT_OK on success
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_CondVarBroadcast)(BML_Context ctx, BML_CondVar condvar);

/* ========== Spin Lock API ========== */

/**
 * @brief Create a spin lock
 * 
 * Spin locks are suitable for very short critical sections where
 * the overhead of a mutex would be prohibitive.
 * 
 * @param[out] out_lock Receives spin lock handle
 * @return BML_RESULT_OK on success
 * 
 * @threadsafe Yes
 * 
 * @warning Do not use for operations that may block or take significant time
 */
typedef BML_Result (*PFN_BML_SpinLockCreate)(BML_Context ctx, BML_Mod owner, BML_SpinLock *out_lock);

/**
 * @brief Destroy a spin lock
 * 
 * @param[in] lock Spin lock handle
 * 
 * @threadsafe No
 */
typedef void (*PFN_BML_SpinLockDestroy)(BML_Context ctx, BML_SpinLock lock);

/**
 * @brief Acquire a spin lock (busy-wait)
 * 
 * @param[in] lock Spin lock handle
 * 
 * @threadsafe Yes
 */
typedef void (*PFN_BML_SpinLockLock)(BML_Context ctx, BML_SpinLock lock);

/**
 * @brief Try to acquire a spin lock (non-blocking)
 * 
 * @param[in] lock Spin lock handle
 * @return BML_TRUE if acquired, BML_FALSE otherwise
 * 
 * @threadsafe Yes
 */
typedef BML_Bool (*PFN_BML_SpinLockTryLock)(BML_Context ctx, BML_SpinLock lock);

/**
 * @brief Release a spin lock
 * 
 * @param[in] lock Spin lock handle
 * 
 * @threadsafe Yes
 */
typedef void (*PFN_BML_SpinLockUnlock)(BML_Context ctx, BML_SpinLock lock);

typedef struct BML_CoreSyncInterface {
    BML_InterfaceHeader header;
    BML_Context Context;
    PFN_BML_MutexCreate MutexCreate;
    PFN_BML_MutexDestroy MutexDestroy;
    PFN_BML_MutexLock MutexLock;
    PFN_BML_MutexTryLock MutexTryLock;
    PFN_BML_MutexLockTimeout MutexLockTimeout;
    PFN_BML_MutexUnlock MutexUnlock;
    PFN_BML_RwLockCreate RwLockCreate;
    PFN_BML_RwLockDestroy RwLockDestroy;
    PFN_BML_RwLockReadLock RwLockReadLock;
    PFN_BML_RwLockTryReadLock RwLockTryReadLock;
    PFN_BML_RwLockReadLockTimeout RwLockReadLockTimeout;
    PFN_BML_RwLockWriteLock RwLockWriteLock;
    PFN_BML_RwLockTryWriteLock RwLockTryWriteLock;
    PFN_BML_RwLockWriteLockTimeout RwLockWriteLockTimeout;
    PFN_BML_RwLockUnlock RwLockUnlock;
    PFN_BML_RwLockReadUnlock RwLockReadUnlock;
    PFN_BML_RwLockWriteUnlock RwLockWriteUnlock;
    PFN_BML_AtomicIncrement32 AtomicIncrement32;
    PFN_BML_AtomicDecrement32 AtomicDecrement32;
    PFN_BML_AtomicAdd32 AtomicAdd32;
    PFN_BML_AtomicCompareExchange32 AtomicCompareExchange32;
    PFN_BML_AtomicExchange32 AtomicExchange32;
    PFN_BML_AtomicLoadPtr AtomicLoadPtr;
    PFN_BML_AtomicStorePtr AtomicStorePtr;
    PFN_BML_AtomicCompareExchangePtr AtomicCompareExchangePtr;
    PFN_BML_SemaphoreCreate SemaphoreCreate;
    PFN_BML_SemaphoreDestroy SemaphoreDestroy;
    PFN_BML_SemaphoreWait SemaphoreWait;
    PFN_BML_SemaphoreSignal SemaphoreSignal;
    PFN_BML_TlsCreate TlsCreate;
    PFN_BML_TlsDestroy TlsDestroy;
    PFN_BML_TlsGet TlsGet;
    PFN_BML_TlsSet TlsSet;
    PFN_BML_CondVarCreate CondVarCreate;
    PFN_BML_CondVarDestroy CondVarDestroy;
    PFN_BML_CondVarWait CondVarWait;
    PFN_BML_CondVarWaitTimeout CondVarWaitTimeout;
    PFN_BML_CondVarSignal CondVarSignal;
    PFN_BML_CondVarBroadcast CondVarBroadcast;
    PFN_BML_SpinLockCreate SpinLockCreate;
    PFN_BML_SpinLockDestroy SpinLockDestroy;
    PFN_BML_SpinLockLock SpinLockLock;
    PFN_BML_SpinLockTryLock SpinLockTryLock;
    PFN_BML_SpinLockUnlock SpinLockUnlock;
} BML_CoreSyncInterface;

BML_END_CDECLS

#endif /* BML_SYNC_H */
