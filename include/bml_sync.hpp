/**
 * @file bml_sync.hpp
 * @brief C++ RAII wrappers for BML synchronization primitives
 */

#ifndef BML_SYNC_HPP
#define BML_SYNC_HPP

#include "bml_sync.h"
#include "bml_assert.hpp"

namespace bml {

    // ========================================================================
    // Mutex
    // ========================================================================

    class Mutex {
    public:
        explicit Mutex(const BML_CoreSyncInterface *iface, BML_Mod owner = nullptr)
            : m_Interface(iface), m_Owner(owner) {
            BML_ASSERT(iface);
            m_Interface->MutexCreate(m_Interface->Context, m_Owner, &m_Handle);
        }

        ~Mutex() {
            if (m_Handle) m_Interface->MutexDestroy(m_Interface->Context, m_Handle);
        }

        void Lock() {
            m_Interface->MutexLock(m_Interface->Context, m_Handle);
        }

        bool TryLock() {
            return m_Interface->MutexTryLock(m_Interface->Context, m_Handle) == BML_TRUE;
        }

        BML_Result LockTimeout(uint32_t timeout_ms) {
            return m_Interface->MutexLockTimeout(m_Interface->Context, m_Handle, timeout_ms);
        }

        void Unlock() {
            m_Interface->MutexUnlock(m_Interface->Context, m_Handle);
        }

        BML_Mutex Handle() const noexcept { return m_Handle; }
        explicit operator bool() const noexcept { return m_Handle != nullptr; }

        Mutex(const Mutex &) = delete;
        Mutex &operator=(const Mutex &) = delete;

        Mutex(Mutex &&other) noexcept
            : m_Interface(other.m_Interface), m_Owner(other.m_Owner), m_Handle(other.m_Handle) {
            other.m_Handle = nullptr;
        }

        Mutex &operator=(Mutex &&other) noexcept {
            if (this != &other) {
                if (m_Handle) m_Interface->MutexDestroy(m_Interface->Context, m_Handle);
                m_Interface = other.m_Interface;
                m_Owner = other.m_Owner;
                m_Handle = other.m_Handle;
                other.m_Handle = nullptr;
            }
            return *this;
        }

    private:
        const BML_CoreSyncInterface *m_Interface = nullptr;
        BML_Mod m_Owner = nullptr;
        BML_Mutex m_Handle = nullptr;
    };

    class MutexGuard {
    public:
        explicit MutexGuard(Mutex &m)
            : m_Mutex(&m) {
            m_Mutex->Lock();
        }

        ~MutexGuard() {
            if (m_Mutex) {
                m_Mutex->Unlock();
            }
        }

        MutexGuard(const MutexGuard &) = delete;
        MutexGuard &operator=(const MutexGuard &) = delete;

    private:
        Mutex *m_Mutex = nullptr;
    };

    // ========================================================================
    // RwLock
    // ========================================================================

    class RwLock {
    public:
        explicit RwLock(const BML_CoreSyncInterface *iface, BML_Mod owner = nullptr)
            : m_Interface(iface), m_Owner(owner) {
            BML_ASSERT(iface);
            m_Interface->RwLockCreate(m_Interface->Context, m_Owner, &m_Handle);
        }

        ~RwLock() {
            if (m_Handle) m_Interface->RwLockDestroy(m_Interface->Context, m_Handle);
        }

        void ReadLock() {
            m_Interface->RwLockReadLock(m_Interface->Context, m_Handle);
        }

        bool TryReadLock() {
            return m_Interface->RwLockTryReadLock(m_Interface->Context, m_Handle) == BML_TRUE;
        }

        BML_Result ReadLockTimeout(uint32_t timeout_ms) {
            return m_Interface->RwLockReadLockTimeout(m_Interface->Context, m_Handle, timeout_ms);
        }

        void WriteLock() {
            m_Interface->RwLockWriteLock(m_Interface->Context, m_Handle);
        }

        bool TryWriteLock() {
            return m_Interface->RwLockTryWriteLock(m_Interface->Context, m_Handle) == BML_TRUE;
        }

        BML_Result WriteLockTimeout(uint32_t timeout_ms) {
            return m_Interface->RwLockWriteLockTimeout(m_Interface->Context, m_Handle, timeout_ms);
        }

        void ReadUnlock() {
            m_Interface->RwLockReadUnlock(m_Interface->Context, m_Handle);
        }

        void WriteUnlock() {
            m_Interface->RwLockWriteUnlock(m_Interface->Context, m_Handle);
        }

        void Unlock() {
            m_Interface->RwLockUnlock(m_Interface->Context, m_Handle);
        }

        BML_RwLock Handle() const noexcept { return m_Handle; }
        explicit operator bool() const noexcept { return m_Handle != nullptr; }

        RwLock(const RwLock &) = delete;
        RwLock &operator=(const RwLock &) = delete;

        RwLock(RwLock &&other) noexcept
            : m_Interface(other.m_Interface), m_Owner(other.m_Owner), m_Handle(other.m_Handle) {
            other.m_Handle = nullptr;
        }

        RwLock &operator=(RwLock &&other) noexcept {
            if (this != &other) {
                if (m_Handle) m_Interface->RwLockDestroy(m_Interface->Context, m_Handle);
                m_Interface = other.m_Interface;
                m_Owner = other.m_Owner;
                m_Handle = other.m_Handle;
                other.m_Handle = nullptr;
            }
            return *this;
        }

    private:
        const BML_CoreSyncInterface *m_Interface = nullptr;
        BML_Mod m_Owner = nullptr;
        BML_RwLock m_Handle = nullptr;
    };

    class ReadGuard {
    public:
        explicit ReadGuard(RwLock &lock)
            : m_Lock(&lock) {
            m_Lock->ReadLock();
        }

        ~ReadGuard() {
            if (m_Lock) {
                m_Lock->ReadUnlock();
            }
        }

        ReadGuard(const ReadGuard &) = delete;
        ReadGuard &operator=(const ReadGuard &) = delete;

    private:
        RwLock *m_Lock = nullptr;
    };

    class WriteGuard {
    public:
        explicit WriteGuard(RwLock &lock)
            : m_Lock(&lock) {
            m_Lock->WriteLock();
        }

        ~WriteGuard() {
            if (m_Lock) {
                m_Lock->WriteUnlock();
            }
        }

        WriteGuard(const WriteGuard &) = delete;
        WriteGuard &operator=(const WriteGuard &) = delete;

    private:
        RwLock *m_Lock = nullptr;
    };

    // ========================================================================
    // Semaphore
    // ========================================================================

    class Semaphore {
    public:
        Semaphore(uint32_t initial, uint32_t max, const BML_CoreSyncInterface *iface, BML_Mod owner = nullptr)
            : m_Interface(iface), m_Owner(owner) {
            BML_ASSERT(iface);
            m_Interface->SemaphoreCreate(m_Interface->Context, m_Owner, initial, max, &m_Handle);
        }

        ~Semaphore() {
            if (m_Handle) m_Interface->SemaphoreDestroy(m_Interface->Context, m_Handle);
        }

        BML_Result Wait(uint32_t timeout_ms = BML_TIMEOUT_INFINITE) {
            return m_Interface->SemaphoreWait(m_Interface->Context, m_Handle, timeout_ms);
        }

        BML_Result Signal(uint32_t count = 1) {
            return m_Interface->SemaphoreSignal(m_Interface->Context, m_Handle, count);
        }

        BML_Semaphore Handle() const noexcept { return m_Handle; }
        explicit operator bool() const noexcept { return m_Handle != nullptr; }

        Semaphore(const Semaphore &) = delete;
        Semaphore &operator=(const Semaphore &) = delete;

        Semaphore(Semaphore &&other) noexcept
            : m_Interface(other.m_Interface), m_Owner(other.m_Owner), m_Handle(other.m_Handle) {
            other.m_Handle = nullptr;
        }

        Semaphore &operator=(Semaphore &&other) noexcept {
            if (this != &other) {
                if (m_Handle) m_Interface->SemaphoreDestroy(m_Interface->Context, m_Handle);
                m_Interface = other.m_Interface;
                m_Owner = other.m_Owner;
                m_Handle = other.m_Handle;
                other.m_Handle = nullptr;
            }
            return *this;
        }

    private:
        const BML_CoreSyncInterface *m_Interface = nullptr;
        BML_Mod m_Owner = nullptr;
        BML_Semaphore m_Handle = nullptr;
    };

    // ========================================================================
    // CondVar
    // ========================================================================

    class CondVar {
    public:
        explicit CondVar(const BML_CoreSyncInterface *iface, BML_Mod owner = nullptr)
            : m_Interface(iface), m_Owner(owner) {
            BML_ASSERT(iface);
            m_Interface->CondVarCreate(m_Interface->Context, m_Owner, &m_Handle);
        }

        ~CondVar() {
            if (m_Handle) m_Interface->CondVarDestroy(m_Interface->Context, m_Handle);
        }

        BML_Result Wait(Mutex &mutex) {
            return m_Interface->CondVarWait(m_Interface->Context, m_Handle, mutex.Handle());
        }

        BML_Result WaitTimeout(Mutex &mutex, uint32_t timeout_ms) {
            return m_Interface->CondVarWaitTimeout(m_Interface->Context, m_Handle, mutex.Handle(), timeout_ms);
        }

        BML_Result Signal() {
            return m_Interface->CondVarSignal(m_Interface->Context, m_Handle);
        }

        BML_Result Broadcast() {
            return m_Interface->CondVarBroadcast(m_Interface->Context, m_Handle);
        }

        BML_CondVar Handle() const noexcept { return m_Handle; }
        explicit operator bool() const noexcept { return m_Handle != nullptr; }

        CondVar(const CondVar &) = delete;
        CondVar &operator=(const CondVar &) = delete;

        CondVar(CondVar &&other) noexcept
            : m_Interface(other.m_Interface), m_Owner(other.m_Owner), m_Handle(other.m_Handle) {
            other.m_Handle = nullptr;
        }

        CondVar &operator=(CondVar &&other) noexcept {
            if (this != &other) {
                if (m_Handle) m_Interface->CondVarDestroy(m_Interface->Context, m_Handle);
                m_Interface = other.m_Interface;
                m_Owner = other.m_Owner;
                m_Handle = other.m_Handle;
                other.m_Handle = nullptr;
            }
            return *this;
        }

    private:
        const BML_CoreSyncInterface *m_Interface = nullptr;
        BML_Mod m_Owner = nullptr;
        BML_CondVar m_Handle = nullptr;
    };

    // ========================================================================
    // SpinLock
    // ========================================================================

    class SpinLock {
    public:
        explicit SpinLock(const BML_CoreSyncInterface *iface, BML_Mod owner = nullptr)
            : m_Interface(iface), m_Owner(owner) {
            BML_ASSERT(iface);
            m_Interface->SpinLockCreate(m_Interface->Context, m_Owner, &m_Handle);
        }

        ~SpinLock() {
            if (m_Handle) m_Interface->SpinLockDestroy(m_Interface->Context, m_Handle);
        }

        void Lock() {
            m_Interface->SpinLockLock(m_Interface->Context, m_Handle);
        }

        bool TryLock() {
            return m_Interface->SpinLockTryLock(m_Interface->Context, m_Handle) == BML_TRUE;
        }

        void Unlock() {
            m_Interface->SpinLockUnlock(m_Interface->Context, m_Handle);
        }

        BML_SpinLock Handle() const noexcept { return m_Handle; }
        explicit operator bool() const noexcept { return m_Handle != nullptr; }

        SpinLock(const SpinLock &) = delete;
        SpinLock &operator=(const SpinLock &) = delete;

        SpinLock(SpinLock &&other) noexcept
            : m_Interface(other.m_Interface), m_Owner(other.m_Owner), m_Handle(other.m_Handle) {
            other.m_Handle = nullptr;
        }

        SpinLock &operator=(SpinLock &&other) noexcept {
            if (this != &other) {
                if (m_Handle) m_Interface->SpinLockDestroy(m_Interface->Context, m_Handle);
                m_Interface = other.m_Interface;
                m_Owner = other.m_Owner;
                m_Handle = other.m_Handle;
                other.m_Handle = nullptr;
            }
            return *this;
        }

    private:
        const BML_CoreSyncInterface *m_Interface = nullptr;
        BML_Mod m_Owner = nullptr;
        BML_SpinLock m_Handle = nullptr;
    };

    class SpinGuard {
    public:
        explicit SpinGuard(SpinLock &lock)
            : m_Lock(&lock) {
            m_Lock->Lock();
        }

        ~SpinGuard() {
            if (m_Lock) {
                m_Lock->Unlock();
            }
        }

        SpinGuard(const SpinGuard &) = delete;
        SpinGuard &operator=(const SpinGuard &) = delete;

    private:
        SpinLock *m_Lock = nullptr;
    };

    // ========================================================================
    // SyncService -- convenience entry point
    // ========================================================================

    class SyncService {
    public:
        SyncService() = default;
        explicit SyncService(const BML_CoreSyncInterface *iface, BML_Mod owner = nullptr) noexcept
            : m_Interface(iface), m_Owner(owner) {}

        Mutex CreateMutex() const {
            return Mutex(m_Interface, m_Owner);
        }

        RwLock CreateRwLock() const {
            return RwLock(m_Interface, m_Owner);
        }

        Semaphore CreateSemaphore(uint32_t initial, uint32_t max) const {
            return Semaphore(initial, max, m_Interface, m_Owner);
        }

        CondVar CreateCondVar() const {
            return CondVar(m_Interface, m_Owner);
        }

        SpinLock CreateSpinLock() const {
            return SpinLock(m_Interface, m_Owner);
        }

        // Atomic operations
        int32_t AtomicIncrement(volatile int32_t *val) const noexcept {
            return m_Interface->AtomicIncrement32(val);
        }
        int32_t AtomicDecrement(volatile int32_t *val) const noexcept {
            return m_Interface->AtomicDecrement32(val);
        }
        int32_t AtomicAdd(volatile int32_t *val, int32_t addend) const noexcept {
            return m_Interface->AtomicAdd32(val, addend);
        }
        int32_t AtomicCompareExchange(volatile int32_t *dest, int32_t exchange, int32_t comparand) const noexcept {
            return m_Interface->AtomicCompareExchange32(dest, exchange, comparand);
        }
        int32_t AtomicExchange(volatile int32_t *dest, int32_t new_value) const noexcept {
            return m_Interface->AtomicExchange32(dest, new_value);
        }
        void *AtomicLoadPtr(void *volatile *ptr) const noexcept {
            return m_Interface->AtomicLoadPtr(ptr);
        }
        void AtomicStorePtr(void *volatile *ptr, void *value) const noexcept {
            m_Interface->AtomicStorePtr(ptr, value);
        }
        void *AtomicCompareExchangePtr(void *volatile *dest, void *exchange, void *comparand) const noexcept {
            return m_Interface->AtomicCompareExchangePtr(dest, exchange, comparand);
        }

        explicit operator bool() const noexcept { return m_Interface != nullptr; }

    private:
        const BML_CoreSyncInterface *m_Interface = nullptr;
        BML_Mod m_Owner = nullptr;
    };

} // namespace bml

#endif /* BML_SYNC_HPP */
