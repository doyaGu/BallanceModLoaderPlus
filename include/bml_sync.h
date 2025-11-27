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
 * 
 * @since 0.4.0
 */

#include "bml_types.h"
#include "bml_errors.h"
#include "bml_version.h"

BML_BEGIN_CDECLS

/* ========== Type-Safe Handle Declarations (Task 1.3) ========== */

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
typedef BML_Result (*PFN_BML_MutexCreate)(BML_Mutex *out_mutex);

/**
 * @brief Destroy a mutex
 * 
 * @param[in] mutex Mutex handle
 * 
 * @threadsafe No (ensure no thread holds the mutex)
 * 
 * @warning Destroying a locked mutex is undefined behavior
 */
typedef void (*PFN_BML_MutexDestroy)(BML_Mutex mutex);

/**
 * @brief Lock a mutex (blocking)
 * 
 * @param[in] mutex Mutex handle
 * 
 * @threadsafe Yes
 * 
 * @note Blocks until mutex is acquired
 * @warning Locking the same mutex twice from same thread causes deadlock
 */
typedef void (*PFN_BML_MutexLock)(BML_Mutex mutex);

/**
 * @brief Try to lock a mutex (non-blocking)
 * 
 * @param[in] mutex Mutex handle
 * @return BML_TRUE if lock acquired, BML_FALSE otherwise
 * 
 * @threadsafe Yes
 */
typedef BML_Bool (*PFN_BML_MutexTryLock)(BML_Mutex mutex);

/**
 * @brief Unlock a mutex
 * 
 * @param[in] mutex Mutex handle
 * 
 * @threadsafe Yes
 * 
 * @warning Must be called by the thread that locked the mutex
 */
typedef void (*PFN_BML_MutexUnlock)(BML_Mutex mutex);

/* ========== Read-Write Lock API ========== */

/**
 * @brief Create a read-write lock
 * 
 * @param[out] out_lock Receives lock handle
 * @return BML_RESULT_OK on success
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_RwLockCreate)(BML_RwLock *out_lock);

/**
 * @brief Destroy a read-write lock
 * 
 * @param[in] lock Lock handle
 * 
 * @threadsafe No
 */
typedef void (*PFN_BML_RwLockDestroy)(BML_RwLock lock);

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
typedef void (*PFN_BML_RwLockReadLock)(BML_RwLock lock);

/**
 * @brief Try to acquire read lock (non-blocking)
 * 
 * @param[in] lock Lock handle
 * @return BML_TRUE if lock acquired, BML_FALSE otherwise
 * 
 * @threadsafe Yes
 */
typedef BML_Bool (*PFN_BML_RwLockTryReadLock)(BML_RwLock lock);

/**
 * @brief Acquire write lock (exclusive access)
 * 
 * @param[in] lock Lock handle
 * 
 * @threadsafe Yes
 * 
 * @note Blocks until all readers and writers release the lock
 */
typedef void (*PFN_BML_RwLockWriteLock)(BML_RwLock lock);

/**
 * @brief Try to acquire write lock (non-blocking)
 * 
 * @param[in] lock Lock handle
 * @return BML_TRUE if lock acquired, BML_FALSE otherwise
 * 
 * @threadsafe Yes
 */
typedef BML_Bool (*PFN_BML_RwLockTryWriteLock)(BML_RwLock lock);

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
typedef void (*PFN_BML_RwLockUnlock)(BML_RwLock lock);

/**
 * @brief Explicitly release a read lock
 *
 * @param[in] lock Lock handle
 *
 * @threadsafe Yes
 *
 * @warning Only call this after acquiring via ReadLock/TryReadLock
 */
typedef void (*PFN_BML_RwLockReadUnlock)(BML_RwLock lock);

/**
 * @brief Explicitly release a write lock
 *
 * @param[in] lock Lock handle
 *
 * @threadsafe Yes
 *
 * @warning Only call this after acquiring via WriteLock/TryWriteLock
 */
typedef void (*PFN_BML_RwLockWriteUnlock)(BML_RwLock lock);

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
typedef void (*PFN_BML_SemaphoreDestroy)(BML_Semaphore semaphore);

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
typedef BML_Result (*PFN_BML_SemaphoreWait)(BML_Semaphore semaphore, uint32_t timeout_ms);

/**
 * @brief Signal semaphore (increment count)
 * 
 * @param[in] semaphore Semaphore handle
 * @param[in] count Number to increment by
 * @return BML_RESULT_OK on success
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_SemaphoreSignal)(BML_Semaphore semaphore, uint32_t count);

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
typedef BML_Result (*PFN_BML_TlsCreate)(BML_TlsDestructor destructor, BML_TlsKey *out_key);

/**
 * @brief Destroy a TLS key
 * 
 * @param[in] key TLS key
 * 
 * @threadsafe No
 */
typedef void (*PFN_BML_TlsDestroy)(BML_TlsKey key);

/**
 * @brief Get thread-local value
 * 
 * @param[in] key TLS key
 * @return Thread-local value (NULL if not set)
 * 
 * @threadsafe Yes
 */
typedef void *(*PFN_BML_TlsGet)(BML_TlsKey key);

/**
 * @brief Set thread-local value
 * 
 * @param[in] key TLS key
 * @param[in] value Value to set (may be NULL)
 * @return BML_RESULT_OK on success
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_TlsSet)(BML_TlsKey key, void *value);

/* ========== Capability Query ========== */

typedef enum BML_SyncCapabilityFlags {
    BML_SYNC_CAP_MUTEX          = 1u << 0,
    BML_SYNC_CAP_RWLOCK         = 1u << 1,
    BML_SYNC_CAP_ATOMICS        = 1u << 2,
    BML_SYNC_CAP_SEMAPHORE      = 1u << 3,
    BML_SYNC_CAP_TLS            = 1u << 4,
    BML_SYNC_CAP_CONDVAR        = 1u << 5,  /**< Condition variable support (Task 3.3) */
    BML_SYNC_CAP_SPINLOCK       = 1u << 6,  /**< Spin lock support (Task 3.3) */
    _BML_SYNC_CAP_FORCE_32BIT   = 0x7FFFFFFF  /**< Force 32-bit enum */
} BML_SyncCapabilityFlags;

typedef struct BML_SyncCaps {
    size_t struct_size;        /**< sizeof(BML_SyncCaps), must be first field */
    BML_Version api_version;   /**< API version */
    uint32_t capability_flags; /**< Bitmask of BML_SyncCapabilityFlags */
} BML_SyncCaps;

/**
 * @def BML_SYNC_CAPS_INIT
 * @brief Static initializer for BML_SyncCaps
 */
#define BML_SYNC_CAPS_INIT { sizeof(BML_SyncCaps), BML_VERSION_INIT(0,0,0), 0 }

typedef BML_Result (*PFN_BML_GetSyncCaps)(BML_SyncCaps *out_caps);

/* ========== Condition Variable API (Task 3.3) ========== */

/**
 * @brief Create a condition variable
 * 
 * @param[out] out_condvar Receives condition variable handle
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_OUT_OF_MEMORY if allocation failed
 * 
 * @threadsafe Yes
 * @since 0.4.0
 */
typedef BML_Result (*PFN_BML_CondVarCreate)(BML_CondVar *out_condvar);

/**
 * @brief Destroy a condition variable
 * 
 * @param[in] condvar Condition variable handle
 * 
 * @threadsafe No (ensure no thread is waiting)
 * @since 0.4.0
 */
typedef void (*PFN_BML_CondVarDestroy)(BML_CondVar condvar);

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
 * @since 0.4.0
 * 
 * @warning Spurious wakeups may occur; always check the condition in a loop
 */
typedef BML_Result (*PFN_BML_CondVarWait)(BML_CondVar condvar, BML_Mutex mutex);

/**
 * @brief Wait on a condition variable with timeout (Task 3.3)
 * 
 * @param[in] condvar Condition variable handle
 * @param[in] mutex Mutex handle (must be locked by caller)
 * @param[in] timeout_ms Timeout in milliseconds
 * @return BML_RESULT_OK when signaled
 * @return BML_RESULT_TIMEOUT if timeout expired
 * 
 * @threadsafe Yes
 * @since 0.4.0
 */
typedef BML_Result (*PFN_BML_CondVarWaitTimeout)(BML_CondVar condvar, BML_Mutex mutex, uint32_t timeout_ms);

/**
 * @brief Signal one waiting thread
 * 
 * @param[in] condvar Condition variable handle
 * @return BML_RESULT_OK on success
 * 
 * @threadsafe Yes
 * @since 0.4.0
 */
typedef BML_Result (*PFN_BML_CondVarSignal)(BML_CondVar condvar);

/**
 * @brief Signal all waiting threads
 * 
 * @param[in] condvar Condition variable handle
 * @return BML_RESULT_OK on success
 * 
 * @threadsafe Yes
 * @since 0.4.0
 */
typedef BML_Result (*PFN_BML_CondVarBroadcast)(BML_CondVar condvar);

/* ========== Spin Lock API (Task 3.3) ========== */

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
 * @since 0.4.0
 * 
 * @warning Do not use for operations that may block or take significant time
 */
typedef BML_Result (*PFN_BML_SpinLockCreate)(BML_SpinLock *out_lock);

/**
 * @brief Destroy a spin lock
 * 
 * @param[in] lock Spin lock handle
 * 
 * @threadsafe No
 * @since 0.4.0
 */
typedef void (*PFN_BML_SpinLockDestroy)(BML_SpinLock lock);

/**
 * @brief Acquire a spin lock (busy-wait)
 * 
 * @param[in] lock Spin lock handle
 * 
 * @threadsafe Yes
 * @since 0.4.0
 */
typedef void (*PFN_BML_SpinLockLock)(BML_SpinLock lock);

/**
 * @brief Try to acquire a spin lock (non-blocking)
 * 
 * @param[in] lock Spin lock handle
 * @return BML_TRUE if acquired, BML_FALSE otherwise
 * 
 * @threadsafe Yes
 * @since 0.4.0
 */
typedef BML_Bool (*PFN_BML_SpinLockTryLock)(BML_SpinLock lock);

/**
 * @brief Release a spin lock
 * 
 * @param[in] lock Spin lock handle
 * 
 * @threadsafe Yes
 * @since 0.4.0
 */
typedef void (*PFN_BML_SpinLockUnlock)(BML_SpinLock lock);

/* ========== Global Function Pointers ========== */

/* Mutex */
extern PFN_BML_MutexCreate          bmlMutexCreate;
extern PFN_BML_MutexDestroy         bmlMutexDestroy;
extern PFN_BML_MutexLock            bmlMutexLock;
extern PFN_BML_MutexTryLock         bmlMutexTryLock;
extern PFN_BML_MutexUnlock          bmlMutexUnlock;

/* Read-Write Lock */
extern PFN_BML_RwLockCreate         bmlRwLockCreate;
extern PFN_BML_RwLockDestroy        bmlRwLockDestroy;
extern PFN_BML_RwLockReadLock       bmlRwLockReadLock;
extern PFN_BML_RwLockTryReadLock    bmlRwLockTryReadLock;
extern PFN_BML_RwLockWriteLock      bmlRwLockWriteLock;
extern PFN_BML_RwLockTryWriteLock   bmlRwLockTryWriteLock;
extern PFN_BML_RwLockUnlock         bmlRwLockUnlock;
extern PFN_BML_RwLockReadUnlock     bmlRwLockReadUnlock;
extern PFN_BML_RwLockWriteUnlock    bmlRwLockWriteUnlock;

/* Atomics */
extern PFN_BML_AtomicIncrement32        bmlAtomicIncrement32;
extern PFN_BML_AtomicDecrement32        bmlAtomicDecrement32;
extern PFN_BML_AtomicAdd32              bmlAtomicAdd32;
extern PFN_BML_AtomicCompareExchange32  bmlAtomicCompareExchange32;
extern PFN_BML_AtomicExchange32         bmlAtomicExchange32;
extern PFN_BML_AtomicLoadPtr            bmlAtomicLoadPtr;
extern PFN_BML_AtomicStorePtr           bmlAtomicStorePtr;
extern PFN_BML_AtomicCompareExchangePtr bmlAtomicCompareExchangePtr;

/* Semaphore */
extern PFN_BML_SemaphoreCreate      bmlSemaphoreCreate;
extern PFN_BML_SemaphoreDestroy     bmlSemaphoreDestroy;
extern PFN_BML_SemaphoreWait        bmlSemaphoreWait;
extern PFN_BML_SemaphoreSignal      bmlSemaphoreSignal;

/* TLS */
extern PFN_BML_TlsCreate            bmlTlsCreate;
extern PFN_BML_TlsDestroy           bmlTlsDestroy;
extern PFN_BML_TlsGet               bmlTlsGet;
extern PFN_BML_TlsSet               bmlTlsSet;

/* Condition Variable (Task 3.3) */
extern PFN_BML_CondVarCreate        bmlCondVarCreate;
extern PFN_BML_CondVarDestroy       bmlCondVarDestroy;
extern PFN_BML_CondVarWait          bmlCondVarWait;
extern PFN_BML_CondVarWaitTimeout   bmlCondVarWaitTimeout;
extern PFN_BML_CondVarSignal        bmlCondVarSignal;
extern PFN_BML_CondVarBroadcast     bmlCondVarBroadcast;

/* Spin Lock (Task 3.3) */
extern PFN_BML_SpinLockCreate       bmlSpinLockCreate;
extern PFN_BML_SpinLockDestroy      bmlSpinLockDestroy;
extern PFN_BML_SpinLockLock         bmlSpinLockLock;
extern PFN_BML_SpinLockTryLock      bmlSpinLockTryLock;
extern PFN_BML_SpinLockUnlock       bmlSpinLockUnlock;

/* Capability Query */
extern PFN_BML_GetSyncCaps          bmlGetSyncCaps;

BML_END_CDECLS

/* ========================================================================
 * Compile-Time Assertions for ABI Stability
 * ======================================================================== */

#ifdef __cplusplus
#include <cstddef>
#define BML_SYNC_OFFSETOF(type, member) offsetof(type, member)
#else
#include <stddef.h>
#define BML_SYNC_OFFSETOF(type, member) offsetof(type, member)
#endif

#if defined(__cplusplus) && __cplusplus >= 201103L
    #define BML_SYNC_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define BML_SYNC_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
    #define BML_SYNC_STATIC_ASSERT_CONCAT_(a, b) a##b
    #define BML_SYNC_STATIC_ASSERT_CONCAT(a, b) BML_SYNC_STATIC_ASSERT_CONCAT_(a, b)
    #define BML_SYNC_STATIC_ASSERT(cond, msg) \
        typedef char BML_SYNC_STATIC_ASSERT_CONCAT(bml_sync_assert_, __LINE__)[(cond) ? 1 : -1]
#endif

/* Verify struct_size is at offset 0 */
BML_SYNC_STATIC_ASSERT(BML_SYNC_OFFSETOF(BML_SyncCaps, struct_size) == 0,
    "BML_SyncCaps.struct_size must be at offset 0");

/* Verify enum sizes are 32-bit */
BML_SYNC_STATIC_ASSERT(sizeof(BML_SyncCapabilityFlags) == sizeof(int32_t),
    "BML_SyncCapabilityFlags must be 32-bit");

#endif /* BML_SYNC_H */
