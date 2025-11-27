/**
 * @file bml_sync.hpp
 * @brief BML C++ Synchronization Primitives Wrapper
 * 
 * Provides RAII-friendly wrappers for BML synchronization primitives
 * including mutexes, read-write locks, semaphores, condition variables,
 * and thread-local storage.
 */

#ifndef BML_SYNC_HPP
#define BML_SYNC_HPP

#include "bml_sync.h"
#include "bml_errors.h"

#include <optional>
#include <utility>
#include <functional>

namespace bml {

// ============================================================================
// Sync Capabilities Query
// ============================================================================

/**
 * @brief Query sync subsystem capabilities
 * @return Capabilities if successful
 */
inline std::optional<BML_SyncCaps> GetSyncCaps() {
    if (!bmlGetSyncCaps) return std::nullopt;
    BML_SyncCaps caps = BML_SYNC_CAPS_INIT;
    if (bmlGetSyncCaps(&caps) == BML_RESULT_OK) {
        return caps;
    }
    return std::nullopt;
}

/**
 * @brief Check if a sync capability is available
 * @param flag Capability flag to check
 * @return true if capability is available
 */
inline bool HasSyncCap(BML_SyncCapabilityFlags flag) {
    auto caps = GetSyncCaps();
    return caps && (caps->capability_flags & flag);
}

// ============================================================================
// Mutex Wrapper
// ============================================================================

/**
 * @brief RAII wrapper for BML_Mutex
 * 
 * Example:
 *   bml::Mutex mutex;
 *   {
 *       bml::LockGuard lock(mutex);
 *       // Critical section
 *   }
 */
class Mutex {
public:
    /**
     * @brief Create a mutex
     * @throws bml::Exception if creation fails
     */
    Mutex() : m_handle(nullptr) {
        if (!bmlMutexCreate) {
            throw Exception(BML_RESULT_NOT_FOUND, "Mutex API unavailable");
        }
        auto result = bmlMutexCreate(&m_handle);
        if (result != BML_RESULT_OK) {
            throw Exception(result, "Failed to create mutex");
        }
    }
    
    /**
     * @brief Destructor - destroys the mutex
     */
    ~Mutex() {
        if (m_handle && bmlMutexDestroy) {
            bmlMutexDestroy(m_handle);
        }
    }
    
    // Non-copyable
    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;
    
    // Movable
    Mutex(Mutex&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }
    
    Mutex& operator=(Mutex&& other) noexcept {
        if (this != &other) {
            if (m_handle && bmlMutexDestroy) {
                bmlMutexDestroy(m_handle);
            }
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }
    
    /**
     * @brief Lock the mutex (blocking)
     */
    void lock() {
        if (bmlMutexLock) {
            bmlMutexLock(m_handle);
        }
    }
    
    /**
     * @brief Try to lock the mutex (non-blocking)
     * @return true if lock acquired
     */
    bool try_lock() {
        return bmlMutexTryLock && bmlMutexTryLock(m_handle) != BML_FALSE;
    }
    
    /**
     * @brief Unlock the mutex
     */
    void unlock() {
        if (bmlMutexUnlock) {
            bmlMutexUnlock(m_handle);
        }
    }
    
    /**
     * @brief Get the underlying handle
     */
    BML_Mutex handle() const noexcept { return m_handle; }
    
private:
    BML_Mutex m_handle;
};

// ============================================================================
// Read-Write Lock Wrapper
// ============================================================================

/**
 * @brief RAII wrapper for BML_RwLock
 * 
 * Example:
 *   bml::RwLock lock;
 *   {
 *       bml::ReadLockGuard read_lock(lock);
 *       // Read operations
 *   }
 *   {
 *       bml::WriteLockGuard write_lock(lock);
 *       // Write operations
 *   }
 */
class RwLock {
public:
    /**
     * @brief Create a read-write lock
     * @throws bml::Exception if creation fails
     */
    RwLock() : m_handle(nullptr) {
        if (!bmlRwLockCreate) {
            throw Exception(BML_RESULT_NOT_FOUND, "RwLock API unavailable");
        }
        auto result = bmlRwLockCreate(&m_handle);
        if (result != BML_RESULT_OK) {
            throw Exception(result, "Failed to create RwLock");
        }
    }
    
    ~RwLock() {
        if (m_handle && bmlRwLockDestroy) {
            bmlRwLockDestroy(m_handle);
        }
    }
    
    // Non-copyable
    RwLock(const RwLock&) = delete;
    RwLock& operator=(const RwLock&) = delete;
    
    // Movable
    RwLock(RwLock&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }
    
    RwLock& operator=(RwLock&& other) noexcept {
        if (this != &other) {
            if (m_handle && bmlRwLockDestroy) {
                bmlRwLockDestroy(m_handle);
            }
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }
    
    void lock_shared() {
        if (bmlRwLockReadLock) bmlRwLockReadLock(m_handle);
    }
    
    bool try_lock_shared() {
        return bmlRwLockTryReadLock && bmlRwLockTryReadLock(m_handle) != BML_FALSE;
    }
    
    void unlock_shared() {
        if (bmlRwLockReadUnlock) bmlRwLockReadUnlock(m_handle);
    }
    
    void lock() {
        if (bmlRwLockWriteLock) bmlRwLockWriteLock(m_handle);
    }
    
    bool try_lock() {
        return bmlRwLockTryWriteLock && bmlRwLockTryWriteLock(m_handle) != BML_FALSE;
    }
    
    void unlock() {
        if (bmlRwLockWriteUnlock) bmlRwLockWriteUnlock(m_handle);
    }
    
    BML_RwLock handle() const noexcept { return m_handle; }
    
private:
    BML_RwLock m_handle;
};

// ============================================================================
// Lock Guards
// ============================================================================

/**
 * @brief RAII lock guard for Mutex (like std::lock_guard)
 */
class LockGuard {
public:
    explicit LockGuard(Mutex& mutex) : m_mutex(mutex) {
        m_mutex.lock();
    }
    
    ~LockGuard() {
        m_mutex.unlock();
    }
    
    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;
    
private:
    Mutex& m_mutex;
};

/**
 * @brief RAII read lock guard for RwLock
 */
class ReadLockGuard {
public:
    explicit ReadLockGuard(RwLock& lock) : m_lock(lock) {
        m_lock.lock_shared();
    }
    
    ~ReadLockGuard() {
        m_lock.unlock_shared();
    }
    
    ReadLockGuard(const ReadLockGuard&) = delete;
    ReadLockGuard& operator=(const ReadLockGuard&) = delete;
    
private:
    RwLock& m_lock;
};

/**
 * @brief RAII write lock guard for RwLock
 */
class WriteLockGuard {
public:
    explicit WriteLockGuard(RwLock& lock) : m_lock(lock) {
        m_lock.lock();
    }
    
    ~WriteLockGuard() {
        m_lock.unlock();
    }
    
    WriteLockGuard(const WriteLockGuard&) = delete;
    WriteLockGuard& operator=(const WriteLockGuard&) = delete;
    
private:
    RwLock& m_lock;
};

// ============================================================================
// Semaphore Wrapper
// ============================================================================

/**
 * @brief RAII wrapper for BML_Semaphore
 */
class Semaphore {
public:
    /**
     * @brief Create a semaphore
     * @param initial_count Initial count value
     * @param max_count Maximum count value
     * @throws bml::Exception if creation fails
     */
    Semaphore(uint32_t initial_count = 0, uint32_t max_count = 0xFFFFFFFF)
        : m_handle(nullptr)
    {
        if (!bmlSemaphoreCreate) {
            throw Exception(BML_RESULT_NOT_FOUND, "Semaphore API unavailable");
        }
        auto result = bmlSemaphoreCreate(initial_count, max_count, &m_handle);
        if (result != BML_RESULT_OK) {
            throw Exception(result, "Failed to create semaphore");
        }
    }
    
    ~Semaphore() {
        if (m_handle && bmlSemaphoreDestroy) {
            bmlSemaphoreDestroy(m_handle);
        }
    }
    
    // Non-copyable
    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;
    
    // Movable
    Semaphore(Semaphore&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }
    
    Semaphore& operator=(Semaphore&& other) noexcept {
        if (this != &other) {
            if (m_handle && bmlSemaphoreDestroy) {
                bmlSemaphoreDestroy(m_handle);
            }
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }
    
    /**
     * @brief Wait on semaphore (decrement count)
     * @param timeout_ms Timeout in milliseconds (0xFFFFFFFF = infinite)
     * @return true if acquired, false on timeout
     */
    bool wait(uint32_t timeout_ms = 0xFFFFFFFF) {
        if (!bmlSemaphoreWait) return false;
        return bmlSemaphoreWait(m_handle, timeout_ms) == BML_RESULT_OK;
    }
    
    /**
     * @brief Signal semaphore (increment count)
     * @param count Number to increment by
     * @return true if successful
     */
    bool signal(uint32_t count = 1) {
        if (!bmlSemaphoreSignal) return false;
        return bmlSemaphoreSignal(m_handle, count) == BML_RESULT_OK;
    }
    
    BML_Semaphore handle() const noexcept { return m_handle; }
    
private:
    BML_Semaphore m_handle;
};

// ============================================================================
// Condition Variable Wrapper
// ============================================================================

/**
 * @brief RAII wrapper for BML_CondVar
 */
class CondVar {
public:
    CondVar() : m_handle(nullptr) {
        if (!bmlCondVarCreate) {
            throw Exception(BML_RESULT_NOT_FOUND, "CondVar API unavailable");
        }
        auto result = bmlCondVarCreate(&m_handle);
        if (result != BML_RESULT_OK) {
            throw Exception(result, "Failed to create condition variable");
        }
    }
    
    ~CondVar() {
        if (m_handle && bmlCondVarDestroy) {
            bmlCondVarDestroy(m_handle);
        }
    }
    
    // Non-copyable
    CondVar(const CondVar&) = delete;
    CondVar& operator=(const CondVar&) = delete;
    
    // Movable
    CondVar(CondVar&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }
    
    CondVar& operator=(CondVar&& other) noexcept {
        if (this != &other) {
            if (m_handle && bmlCondVarDestroy) {
                bmlCondVarDestroy(m_handle);
            }
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }
    
    /**
     * @brief Wait on condition variable
     * @param mutex Mutex to release while waiting
     */
    void wait(Mutex& mutex) {
        if (bmlCondVarWait) {
            bmlCondVarWait(m_handle, mutex.handle());
        }
    }
    
    /**
     * @brief Wait on condition variable with timeout
     * @param mutex Mutex to release while waiting
     * @param timeout_ms Timeout in milliseconds
     * @return true if signaled, false on timeout
     */
    bool wait_for(Mutex& mutex, uint32_t timeout_ms) {
        if (!bmlCondVarWaitTimeout) return false;
        return bmlCondVarWaitTimeout(m_handle, mutex.handle(), timeout_ms) == BML_RESULT_OK;
    }
    
    /**
     * @brief Wait with predicate
     * @tparam Pred Predicate type
     * @param mutex Mutex to release while waiting
     * @param pred Predicate to check
     */
    template<typename Pred>
    void wait(Mutex& mutex, Pred pred) {
        while (!pred()) {
            wait(mutex);
        }
    }
    
    /**
     * @brief Signal one waiting thread
     */
    void notify_one() {
        if (bmlCondVarSignal) {
            bmlCondVarSignal(m_handle);
        }
    }
    
    /**
     * @brief Signal all waiting threads
     */
    void notify_all() {
        if (bmlCondVarBroadcast) {
            bmlCondVarBroadcast(m_handle);
        }
    }
    
    BML_CondVar handle() const noexcept { return m_handle; }
    
private:
    BML_CondVar m_handle;
};

// ============================================================================
// Spin Lock Wrapper
// ============================================================================

/**
 * @brief RAII wrapper for BML_SpinLock
 */
class SpinLock {
public:
    SpinLock() : m_handle(nullptr) {
        if (!bmlSpinLockCreate) {
            throw Exception(BML_RESULT_NOT_FOUND, "SpinLock API unavailable");
        }
        auto result = bmlSpinLockCreate(&m_handle);
        if (result != BML_RESULT_OK) {
            throw Exception(result, "Failed to create spin lock");
        }
    }
    
    ~SpinLock() {
        if (m_handle && bmlSpinLockDestroy) {
            bmlSpinLockDestroy(m_handle);
        }
    }
    
    // Non-copyable
    SpinLock(const SpinLock&) = delete;
    SpinLock& operator=(const SpinLock&) = delete;
    
    // Movable
    SpinLock(SpinLock&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }
    
    SpinLock& operator=(SpinLock&& other) noexcept {
        if (this != &other) {
            if (m_handle && bmlSpinLockDestroy) {
                bmlSpinLockDestroy(m_handle);
            }
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }
    
    void lock() {
        if (bmlSpinLockLock) bmlSpinLockLock(m_handle);
    }
    
    bool try_lock() {
        return bmlSpinLockTryLock && bmlSpinLockTryLock(m_handle) != BML_FALSE;
    }
    
    void unlock() {
        if (bmlSpinLockUnlock) bmlSpinLockUnlock(m_handle);
    }
    
    BML_SpinLock handle() const noexcept { return m_handle; }
    
private:
    BML_SpinLock m_handle;
};

/**
 * @brief RAII lock guard for SpinLock
 */
class SpinLockGuard {
public:
    explicit SpinLockGuard(SpinLock& lock) : m_lock(lock) {
        m_lock.lock();
    }
    
    ~SpinLockGuard() {
        m_lock.unlock();
    }
    
    SpinLockGuard(const SpinLockGuard&) = delete;
    SpinLockGuard& operator=(const SpinLockGuard&) = delete;
    
private:
    SpinLock& m_lock;
};

// ============================================================================
// Thread-Local Storage Wrapper
// ============================================================================

/**
 * @brief RAII wrapper for thread-local storage
 * 
 * Example:
 *   bml::ThreadLocal<MyData> tls([](void* ptr) { delete static_cast<MyData*>(ptr); });
 *   tls.set(new MyData());
 *   MyData* data = tls.get<MyData>();
 */
class ThreadLocal {
public:
    /**
     * @brief Create TLS key
     * @param destructor Optional destructor for values
     * @throws bml::Exception if creation fails
     */
    explicit ThreadLocal(BML_TlsDestructor destructor = nullptr)
        : m_key(nullptr)
    {
        if (!bmlTlsCreate) {
            throw Exception(BML_RESULT_NOT_FOUND, "TLS API unavailable");
        }
        auto result = bmlTlsCreate(destructor, &m_key);
        if (result != BML_RESULT_OK) {
            throw Exception(result, "Failed to create TLS key");
        }
    }
    
    ~ThreadLocal() {
        if (m_key && bmlTlsDestroy) {
            bmlTlsDestroy(m_key);
        }
    }
    
    // Non-copyable
    ThreadLocal(const ThreadLocal&) = delete;
    ThreadLocal& operator=(const ThreadLocal&) = delete;
    
    // Movable
    ThreadLocal(ThreadLocal&& other) noexcept : m_key(other.m_key) {
        other.m_key = nullptr;
    }
    
    ThreadLocal& operator=(ThreadLocal&& other) noexcept {
        if (this != &other) {
            if (m_key && bmlTlsDestroy) {
                bmlTlsDestroy(m_key);
            }
            m_key = other.m_key;
            other.m_key = nullptr;
        }
        return *this;
    }
    
    /**
     * @brief Get thread-local value
     * @return Value pointer (nullptr if not set)
     */
    void* get() const {
        return bmlTlsGet ? bmlTlsGet(m_key) : nullptr;
    }
    
    /**
     * @brief Get thread-local value (typed)
     * @tparam T Value type
     * @return Typed pointer
     */
    template<typename T>
    T* get() const {
        return static_cast<T*>(get());
    }
    
    /**
     * @brief Set thread-local value
     * @param value Value to set
     * @return true if successful
     */
    bool set(void* value) {
        if (!bmlTlsSet) return false;
        return bmlTlsSet(m_key, value) == BML_RESULT_OK;
    }
    
    BML_TlsKey key() const noexcept { return m_key; }
    
private:
    BML_TlsKey m_key;
};

// ============================================================================
// Atomic Helpers
// ============================================================================

namespace atomic {

/**
 * @brief Atomically increment a 32-bit integer
 * @param value Pointer to integer
 * @return New value after increment
 */
inline int32_t Increment32(volatile int32_t* value) {
    return bmlAtomicIncrement32 ? bmlAtomicIncrement32(value) : ++(*value);
}

/**
 * @brief Atomically decrement a 32-bit integer
 * @param value Pointer to integer
 * @return New value after decrement
 */
inline int32_t Decrement32(volatile int32_t* value) {
    return bmlAtomicDecrement32 ? bmlAtomicDecrement32(value) : --(*value);
}

/**
 * @brief Atomically add to a 32-bit integer
 * @param value Pointer to integer
 * @param addend Value to add
 * @return Previous value before addition
 */
inline int32_t Add32(volatile int32_t* value, int32_t addend) {
    if (bmlAtomicAdd32) return bmlAtomicAdd32(value, addend);
    int32_t old = *value;
    *value += addend;
    return old;
}

/**
 * @brief Atomically compare and exchange
 * @param dest Pointer to destination
 * @param exchange Value to set if comparison succeeds
 * @param comparand Value to compare against
 * @return Previous value of *dest
 */
inline int32_t CompareExchange32(volatile int32_t* dest, int32_t exchange, int32_t comparand) {
    if (bmlAtomicCompareExchange32) {
        return bmlAtomicCompareExchange32(dest, exchange, comparand);
    }
    int32_t old = *dest;
    if (old == comparand) *dest = exchange;
    return old;
}

/**
 * @brief Atomically exchange values
 * @param dest Pointer to destination
 * @param new_value New value to set
 * @return Previous value of *dest
 */
inline int32_t Exchange32(volatile int32_t* dest, int32_t new_value) {
    if (bmlAtomicExchange32) return bmlAtomicExchange32(dest, new_value);
    int32_t old = *dest;
    *dest = new_value;
    return old;
}

/**
 * @brief Atomically load a pointer
 * @param ptr Pointer to pointer
 * @return Current pointer value
 */
inline void* LoadPtr(void* volatile* ptr) {
    return bmlAtomicLoadPtr ? bmlAtomicLoadPtr(ptr) : *ptr;
}

/**
 * @brief Atomically store a pointer
 * @param ptr Pointer to pointer
 * @param value New pointer value
 */
inline void StorePtr(void* volatile* ptr, void* value) {
    if (bmlAtomicStorePtr) bmlAtomicStorePtr(ptr, value);
    else *ptr = value;
}

/**
 * @brief Atomically compare and exchange pointers
 * @param dest Pointer to destination pointer
 * @param exchange Pointer to set if comparison succeeds
 * @param comparand Pointer to compare against
 * @return Previous pointer value
 */
inline void* CompareExchangePtr(void* volatile* dest, void* exchange, void* comparand) {
    if (bmlAtomicCompareExchangePtr) {
        return bmlAtomicCompareExchangePtr(dest, exchange, comparand);
    }
    void* old = *dest;
    if (old == comparand) *dest = exchange;
    return old;
}

} // namespace atomic

} // namespace bml

#endif /* BML_SYNC_HPP */
