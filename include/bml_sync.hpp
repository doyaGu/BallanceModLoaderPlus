/**
 * @file bml_sync.hpp
 * @brief C++ RAII wrappers for BML synchronization primitives
 */

#ifndef BML_SYNC_HPP
#define BML_SYNC_HPP

#include "bml_builtin_interfaces.h"

namespace bml {

    // ========================================================================
    // Mutex
    // ========================================================================

    class Mutex {
    public:
        explicit Mutex(const BML_CoreSyncInterface *iface)
            : m_Interface(iface) {
            if (m_Interface && m_Interface->MutexCreate) {
                m_Interface->MutexCreate(&m_Handle);
            }
        }

        ~Mutex() {
            if (m_Interface && m_Interface->MutexDestroy) {
                m_Interface->MutexDestroy(m_Handle);
            }
        }

        void Lock() {
            if (m_Interface && m_Interface->MutexLock) {
                m_Interface->MutexLock(m_Handle);
            }
        }

        bool TryLock() {
            if (m_Interface && m_Interface->MutexTryLock) {
                return m_Interface->MutexTryLock(m_Handle) == BML_TRUE;
            }
            return false;
        }

        BML_Result LockTimeout(uint32_t timeout_ms) {
            if (m_Interface && m_Interface->MutexLockTimeout) {
                return m_Interface->MutexLockTimeout(m_Handle, timeout_ms);
            }
            return BML_RESULT_NOT_INITIALIZED;
        }

        void Unlock() {
            if (m_Interface && m_Interface->MutexUnlock) {
                m_Interface->MutexUnlock(m_Handle);
            }
        }

        BML_Mutex Handle() const noexcept { return m_Handle; }
        explicit operator bool() const noexcept { return m_Handle != nullptr; }

        Mutex(const Mutex &) = delete;
        Mutex &operator=(const Mutex &) = delete;

        Mutex(Mutex &&other) noexcept
            : m_Interface(other.m_Interface), m_Handle(other.m_Handle) {
            other.m_Handle = nullptr;
        }

        Mutex &operator=(Mutex &&other) noexcept {
            if (this != &other) {
                if (m_Interface && m_Interface->MutexDestroy) {
                    m_Interface->MutexDestroy(m_Handle);
                }
                m_Interface = other.m_Interface;
                m_Handle = other.m_Handle;
                other.m_Handle = nullptr;
            }
            return *this;
        }

    private:
        const BML_CoreSyncInterface *m_Interface = nullptr;
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
        explicit RwLock(const BML_CoreSyncInterface *iface)
            : m_Interface(iface) {
            if (m_Interface && m_Interface->RwLockCreate) {
                m_Interface->RwLockCreate(&m_Handle);
            }
        }

        ~RwLock() {
            if (m_Interface && m_Interface->RwLockDestroy) {
                m_Interface->RwLockDestroy(m_Handle);
            }
        }

        void ReadLock() {
            if (m_Interface && m_Interface->RwLockReadLock) {
                m_Interface->RwLockReadLock(m_Handle);
            }
        }

        bool TryReadLock() {
            if (m_Interface && m_Interface->RwLockTryReadLock) {
                return m_Interface->RwLockTryReadLock(m_Handle) == BML_TRUE;
            }
            return false;
        }

        BML_Result ReadLockTimeout(uint32_t timeout_ms) {
            if (m_Interface && m_Interface->RwLockReadLockTimeout) {
                return m_Interface->RwLockReadLockTimeout(m_Handle, timeout_ms);
            }
            return BML_RESULT_NOT_INITIALIZED;
        }

        void WriteLock() {
            if (m_Interface && m_Interface->RwLockWriteLock) {
                m_Interface->RwLockWriteLock(m_Handle);
            }
        }

        bool TryWriteLock() {
            if (m_Interface && m_Interface->RwLockTryWriteLock) {
                return m_Interface->RwLockTryWriteLock(m_Handle) == BML_TRUE;
            }
            return false;
        }

        BML_Result WriteLockTimeout(uint32_t timeout_ms) {
            if (m_Interface && m_Interface->RwLockWriteLockTimeout) {
                return m_Interface->RwLockWriteLockTimeout(m_Handle, timeout_ms);
            }
            return BML_RESULT_NOT_INITIALIZED;
        }

        void ReadUnlock() {
            if (m_Interface && m_Interface->RwLockReadUnlock) {
                m_Interface->RwLockReadUnlock(m_Handle);
            }
        }

        void WriteUnlock() {
            if (m_Interface && m_Interface->RwLockWriteUnlock) {
                m_Interface->RwLockWriteUnlock(m_Handle);
            }
        }

        void Unlock() {
            if (m_Interface && m_Interface->RwLockUnlock) {
                m_Interface->RwLockUnlock(m_Handle);
            }
        }

        BML_RwLock Handle() const noexcept { return m_Handle; }
        explicit operator bool() const noexcept { return m_Handle != nullptr; }

        RwLock(const RwLock &) = delete;
        RwLock &operator=(const RwLock &) = delete;

        RwLock(RwLock &&other) noexcept
            : m_Interface(other.m_Interface), m_Handle(other.m_Handle) {
            other.m_Handle = nullptr;
        }

        RwLock &operator=(RwLock &&other) noexcept {
            if (this != &other) {
                if (m_Interface && m_Interface->RwLockDestroy) {
                    m_Interface->RwLockDestroy(m_Handle);
                }
                m_Interface = other.m_Interface;
                m_Handle = other.m_Handle;
                other.m_Handle = nullptr;
            }
            return *this;
        }

    private:
        const BML_CoreSyncInterface *m_Interface = nullptr;
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
        Semaphore(uint32_t initial, uint32_t max, const BML_CoreSyncInterface *iface)
            : m_Interface(iface) {
            if (m_Interface && m_Interface->SemaphoreCreate) {
                m_Interface->SemaphoreCreate(initial, max, &m_Handle);
            }
        }

        ~Semaphore() {
            if (m_Interface && m_Interface->SemaphoreDestroy) {
                m_Interface->SemaphoreDestroy(m_Handle);
            }
        }

        BML_Result Wait(uint32_t timeout_ms = BML_TIMEOUT_INFINITE) {
            if (m_Interface && m_Interface->SemaphoreWait) {
                return m_Interface->SemaphoreWait(m_Handle, timeout_ms);
            }
            return BML_RESULT_NOT_INITIALIZED;
        }

        BML_Result Signal(uint32_t count = 1) {
            if (m_Interface && m_Interface->SemaphoreSignal) {
                return m_Interface->SemaphoreSignal(m_Handle, count);
            }
            return BML_RESULT_NOT_INITIALIZED;
        }

        BML_Semaphore Handle() const noexcept { return m_Handle; }
        explicit operator bool() const noexcept { return m_Handle != nullptr; }

        Semaphore(const Semaphore &) = delete;
        Semaphore &operator=(const Semaphore &) = delete;

        Semaphore(Semaphore &&other) noexcept
            : m_Interface(other.m_Interface), m_Handle(other.m_Handle) {
            other.m_Handle = nullptr;
        }

        Semaphore &operator=(Semaphore &&other) noexcept {
            if (this != &other) {
                if (m_Interface && m_Interface->SemaphoreDestroy) {
                    m_Interface->SemaphoreDestroy(m_Handle);
                }
                m_Interface = other.m_Interface;
                m_Handle = other.m_Handle;
                other.m_Handle = nullptr;
            }
            return *this;
        }

    private:
        const BML_CoreSyncInterface *m_Interface = nullptr;
        BML_Semaphore m_Handle = nullptr;
    };

    // ========================================================================
    // CondVar
    // ========================================================================

    class CondVar {
    public:
        explicit CondVar(const BML_CoreSyncInterface *iface)
            : m_Interface(iface) {
            if (m_Interface && m_Interface->CondVarCreate) {
                m_Interface->CondVarCreate(&m_Handle);
            }
        }

        ~CondVar() {
            if (m_Interface && m_Interface->CondVarDestroy) {
                m_Interface->CondVarDestroy(m_Handle);
            }
        }

        BML_Result Wait(Mutex &mutex) {
            if (m_Interface && m_Interface->CondVarWait) {
                return m_Interface->CondVarWait(m_Handle, mutex.Handle());
            }
            return BML_RESULT_NOT_INITIALIZED;
        }

        BML_Result WaitTimeout(Mutex &mutex, uint32_t timeout_ms) {
            if (m_Interface && m_Interface->CondVarWaitTimeout) {
                return m_Interface->CondVarWaitTimeout(m_Handle, mutex.Handle(), timeout_ms);
            }
            return BML_RESULT_NOT_INITIALIZED;
        }

        BML_Result Signal() {
            if (m_Interface && m_Interface->CondVarSignal) {
                return m_Interface->CondVarSignal(m_Handle);
            }
            return BML_RESULT_NOT_INITIALIZED;
        }

        BML_Result Broadcast() {
            if (m_Interface && m_Interface->CondVarBroadcast) {
                return m_Interface->CondVarBroadcast(m_Handle);
            }
            return BML_RESULT_NOT_INITIALIZED;
        }

        BML_CondVar Handle() const noexcept { return m_Handle; }
        explicit operator bool() const noexcept { return m_Handle != nullptr; }

        CondVar(const CondVar &) = delete;
        CondVar &operator=(const CondVar &) = delete;

        CondVar(CondVar &&other) noexcept
            : m_Interface(other.m_Interface), m_Handle(other.m_Handle) {
            other.m_Handle = nullptr;
        }

        CondVar &operator=(CondVar &&other) noexcept {
            if (this != &other) {
                if (m_Interface && m_Interface->CondVarDestroy) {
                    m_Interface->CondVarDestroy(m_Handle);
                }
                m_Interface = other.m_Interface;
                m_Handle = other.m_Handle;
                other.m_Handle = nullptr;
            }
            return *this;
        }

    private:
        const BML_CoreSyncInterface *m_Interface = nullptr;
        BML_CondVar m_Handle = nullptr;
    };

    // ========================================================================
    // SpinLock
    // ========================================================================

    class SpinLock {
    public:
        explicit SpinLock(const BML_CoreSyncInterface *iface)
            : m_Interface(iface) {
            if (m_Interface && m_Interface->SpinLockCreate) {
                m_Interface->SpinLockCreate(&m_Handle);
            }
        }

        ~SpinLock() {
            if (m_Interface && m_Interface->SpinLockDestroy) {
                m_Interface->SpinLockDestroy(m_Handle);
            }
        }

        void Lock() {
            if (m_Interface && m_Interface->SpinLockLock) {
                m_Interface->SpinLockLock(m_Handle);
            }
        }

        bool TryLock() {
            if (m_Interface && m_Interface->SpinLockTryLock) {
                return m_Interface->SpinLockTryLock(m_Handle) == BML_TRUE;
            }
            return false;
        }

        void Unlock() {
            if (m_Interface && m_Interface->SpinLockUnlock) {
                m_Interface->SpinLockUnlock(m_Handle);
            }
        }

        BML_SpinLock Handle() const noexcept { return m_Handle; }
        explicit operator bool() const noexcept { return m_Handle != nullptr; }

        SpinLock(const SpinLock &) = delete;
        SpinLock &operator=(const SpinLock &) = delete;

        SpinLock(SpinLock &&other) noexcept
            : m_Interface(other.m_Interface), m_Handle(other.m_Handle) {
            other.m_Handle = nullptr;
        }

        SpinLock &operator=(SpinLock &&other) noexcept {
            if (this != &other) {
                if (m_Interface && m_Interface->SpinLockDestroy) {
                    m_Interface->SpinLockDestroy(m_Handle);
                }
                m_Interface = other.m_Interface;
                m_Handle = other.m_Handle;
                other.m_Handle = nullptr;
            }
            return *this;
        }

    private:
        const BML_CoreSyncInterface *m_Interface = nullptr;
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
        explicit SyncService(const BML_CoreSyncInterface *iface) noexcept
            : m_Interface(iface) {}

        Mutex CreateMutex() const {
            return Mutex(m_Interface);
        }

        RwLock CreateRwLock() const {
            return RwLock(m_Interface);
        }

        Semaphore CreateSemaphore(uint32_t initial, uint32_t max) const {
            return Semaphore(initial, max, m_Interface);
        }

        CondVar CreateCondVar() const {
            return CondVar(m_Interface);
        }

        SpinLock CreateSpinLock() const {
            return SpinLock(m_Interface);
        }

        explicit operator bool() const noexcept { return m_Interface != nullptr; }

    private:
        const BML_CoreSyncInterface *m_Interface = nullptr;
    };

} // namespace bml

#endif /* BML_SYNC_HPP */
