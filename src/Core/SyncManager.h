#ifndef BML_CORE_SYNC_MANAGER_H
#define BML_CORE_SYNC_MANAGER_H

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "bml_sync.h"

namespace BML::Core {

    class DeadlockDetector;
    /**
     * @brief Mutex implementation using Windows CRITICAL_SECTION
     */
    struct MutexImpl {
        CRITICAL_SECTION cs;

        MutexImpl() {
            InitializeCriticalSection(&cs);
        }

        ~MutexImpl() {
            DeleteCriticalSection(&cs);
        }

        MutexImpl(const MutexImpl &) = delete;
        MutexImpl &operator=(const MutexImpl &) = delete;
    };

    /**
     * @brief Read-Write Lock implementation using Windows SRW Lock
     *
     * Tracks lock type per-thread using TLS to support unified unlock API.
     * Windows SRW locks require knowing whether it was read or write locked
     * for proper release (ReleaseSRWLockShared vs ReleaseSRWLockExclusive).
     */
    struct RwLockImpl {
        SRWLOCK srw;
        DWORD tls_index;  // TLS slot to track per-thread state

        struct ThreadState {
            uint16_t read_depth{0};
            uint16_t write_depth{0};
        };

        static constexpr uint16_t kMaxRecursionDepth = 0xFFFF;

        RwLockImpl() : tls_index(TlsAlloc()) {
            InitializeSRWLock(&srw);
        }

        ~RwLockImpl() {
            if (tls_index != TLS_OUT_OF_INDEXES) {
                TlsFree(tls_index);
            }
        }

        RwLockImpl(const RwLockImpl &) = delete;
        RwLockImpl &operator=(const RwLockImpl &) = delete;

        ThreadState GetThreadState() const {
            if (tls_index == TLS_OUT_OF_INDEXES) {
                return {};
            }
            ULONG_PTR raw = reinterpret_cast<ULONG_PTR>(TlsGetValue(tls_index));
            ThreadState state{};
            state.read_depth = static_cast<uint16_t>(raw & 0xFFFFu);
            state.write_depth = static_cast<uint16_t>((raw >> 16u) & 0xFFFFu);
            return state;
        }

        void SetThreadState(const ThreadState &state) {
            if (tls_index == TLS_OUT_OF_INDEXES) {
                return;
            }

            if (state.read_depth == 0 && state.write_depth == 0) {
                TlsSetValue(tls_index, nullptr);
                return;
            }

            ULONG_PTR raw = (static_cast<ULONG_PTR>(state.write_depth) << 16u) |
                            static_cast<ULONG_PTR>(state.read_depth);
            TlsSetValue(tls_index, reinterpret_cast<LPVOID>(raw));
        }
    };

    /**
     * @brief Semaphore implementation using Windows Semaphore
     */
    struct SemaphoreImpl {
        HANDLE handle;
        uint32_t max_count;

        SemaphoreImpl(uint32_t initial, uint32_t maximum)
            : handle(CreateSemaphoreW(nullptr, initial, maximum, nullptr)), max_count(maximum) {}

        ~SemaphoreImpl() {
            if (handle) {
                CloseHandle(handle);
            }
        }

        SemaphoreImpl(const SemaphoreImpl &) = delete;
        SemaphoreImpl &operator=(const SemaphoreImpl &) = delete;
    };

    /**
     * @brief Condition variable implementation using Windows CONDITION_VARIABLE
     */
    struct CondVarImpl {
        CONDITION_VARIABLE cv;

        CondVarImpl() {
            InitializeConditionVariable(&cv);
        }

        // No destructor needed - CONDITION_VARIABLE is statically allocated
        ~CondVarImpl() = default;

        CondVarImpl(const CondVarImpl &) = delete;
        CondVarImpl &operator=(const CondVarImpl &) = delete;
    };

    /**
     * @brief Spin lock implementation using atomic flag
     *
     * Spin locks are efficient for very short critical sections where
     * context switching overhead exceeds spinning time.
     */
    struct SpinLockImpl {
        std::atomic<uint32_t> next_ticket{0};
        std::atomic<uint32_t> now_serving{0};

        SpinLockImpl() = default;
        ~SpinLockImpl() = default;

        SpinLockImpl(const SpinLockImpl &) = delete;
        SpinLockImpl &operator=(const SpinLockImpl &) = delete;
    };

    /**
     * @brief Wrapper for TLS values to support destructor callbacks
     *
     * Must be declared before TlsKeyImpl because FlsCallback uses it.
     */
    struct TlsValueWrapper {
        void *value;
        BML_TlsDestructor destructor;
    };

    /**
     * @brief TLS key implementation using Windows FLS (Fiber Local Storage)
     *
     * Uses FLS instead of TLS because FLS supports automatic destructor callbacks
     * when threads exit, while TLS does not. This ensures proper cleanup of
     * thread-local resources.
     */
    struct TlsKeyImpl {
        DWORD fls_index;
        BML_TlsDestructor destructor;

        TlsKeyImpl(BML_TlsDestructor dtor)
            : fls_index(FlsAlloc(dtor ? FlsCallback : nullptr)), destructor(dtor) {
        }

        ~TlsKeyImpl() {
            if (fls_index != FLS_OUT_OF_INDEXES) {
                FlsFree(fls_index);
            }
        }

        TlsKeyImpl(const TlsKeyImpl &) = delete;
        TlsKeyImpl &operator=(const TlsKeyImpl &) = delete;

        // Static callback wrapper for FLS - invoked automatically on thread exit
        static void WINAPI FlsCallback(PVOID data) {
            // The FLS callback receives the stored value when thread exits
            // The wrapper contains both the value and the destructor pointer
            if (data) {
                auto *wrapper = static_cast<TlsValueWrapper *>(data);
                if (wrapper->destructor && wrapper->value) {
                    wrapper->destructor(wrapper->value);
                }
                delete wrapper;
            }
        }
    };

    /**
     * @brief Synchronization primitives manager
     */
    class SyncManager {
    public:
        static SyncManager &Instance();

        SyncManager(const SyncManager &) = delete;
        SyncManager &operator=(const SyncManager &) = delete;

        // Mutex operations
        BML_Result CreateMutex(BML_Mutex *out_mutex);
        void DestroyMutex(BML_Mutex mutex);
        void LockMutex(BML_Mutex mutex);
        BML_Bool TryLockMutex(BML_Mutex mutex);
        void UnlockMutex(BML_Mutex mutex);

        // RwLock operations
        BML_Result CreateRwLock(BML_RwLock *out_lock);
        void DestroyRwLock(BML_RwLock lock);
        void ReadLockRwLock(BML_RwLock lock);
        BML_Bool TryReadLockRwLock(BML_RwLock lock);
        void WriteLockRwLock(BML_RwLock lock);
        BML_Bool TryWriteLockRwLock(BML_RwLock lock);
        void UnlockRwLock(BML_RwLock lock);
        void ReadUnlockRwLock(BML_RwLock lock);
        void WriteUnlockRwLock(BML_RwLock lock);

        // Atomic operations (inline for performance)
        static int32_t AtomicIncrement32(volatile int32_t *value);
        static int32_t AtomicDecrement32(volatile int32_t *value);
        static int32_t AtomicAdd32(volatile int32_t *value, int32_t addend);
        static int32_t AtomicCompareExchange32(volatile int32_t *dest, int32_t exchange, int32_t comparand);
        static int32_t AtomicExchange32(volatile int32_t *dest, int32_t new_value);
        static void *AtomicLoadPtr(void *volatile*ptr);
        static void AtomicStorePtr(void *volatile*ptr, void *value);
        static void *AtomicCompareExchangePtr(void *volatile*dest, void *exchange, void *comparand);

        // Semaphore operations
        BML_Result CreateSemaphore(uint32_t initial_count, uint32_t max_count, BML_Semaphore *out_semaphore);
        void DestroySemaphore(BML_Semaphore semaphore);
        BML_Result WaitSemaphore(BML_Semaphore semaphore, uint32_t timeout_ms);
        BML_Result SignalSemaphore(BML_Semaphore semaphore, uint32_t count);

        // TLS operations
        BML_Result CreateTls(BML_TlsDestructor destructor, BML_TlsKey *out_key);
        void DestroyTls(BML_TlsKey key);
        void *GetTls(BML_TlsKey key);
        BML_Result SetTls(BML_TlsKey key, void *value);

        // CondVar operations
        BML_Result CreateCondVar(BML_CondVar *out_condvar);
        void DestroyCondVar(BML_CondVar condvar);
        BML_Result WaitCondVar(BML_CondVar condvar, BML_Mutex mutex);
        BML_Result WaitCondVarTimeout(BML_CondVar condvar, BML_Mutex mutex, uint32_t timeout_ms);
        BML_Result SignalCondVar(BML_CondVar condvar);
        BML_Result BroadcastCondVar(BML_CondVar condvar);

        // SpinLock operations
        BML_Result CreateSpinLock(BML_SpinLock *out_lock);
        void DestroySpinLock(BML_SpinLock lock);
        void LockSpinLock(BML_SpinLock lock);
        BML_Bool TryLockSpinLock(BML_SpinLock lock);
        void UnlockSpinLock(BML_SpinLock lock);

        // Capabilities
        BML_Result GetCaps(BML_SyncCaps *out_caps);

    private:
        SyncManager();
        ~SyncManager() = default;

        bool IsValidMutex(BML_Mutex mutex) const;
        bool IsValidRwLock(BML_RwLock lock) const;
        bool IsValidSemaphore(BML_Semaphore semaphore) const;
        bool IsValidTlsKey(BML_TlsKey key) const;
        bool IsValidCondVar(BML_CondVar condvar) const;
        bool IsValidSpinLock(BML_SpinLock lock) const;

        bool ValidateMutexHandle(BML_Mutex mutex, const char *api) const;
        bool ValidateRwLockHandle(BML_RwLock lock, const char *api) const;
        bool ValidateSemaphoreHandle(BML_Semaphore semaphore, const char *api) const;
        bool ValidateTlsHandle(BML_TlsKey key, const char *api) const;
        bool ValidateCondVarHandle(BML_CondVar condvar, const char *api) const;
        bool ValidateSpinLockHandle(BML_SpinLock lock, const char *api) const;

        void ReportLockMisuse(const char *api, const char *message) const;
        BML_Result ReportDeadlock(const char *api) const;

        std::mutex m_mutex_registry_lock;
        std::vector<MutexImpl *> m_mutexes;

        std::mutex m_rwlock_registry_lock;
        std::vector<RwLockImpl *> m_rwlocks;

        std::mutex m_semaphore_registry_lock;
        std::vector<SemaphoreImpl *> m_semaphores;

        std::mutex m_tls_registry_lock;
        std::vector<TlsKeyImpl *> m_tls_keys;

        std::mutex m_condvar_registry_lock;
        std::vector<CondVarImpl *> m_condvars;

        std::mutex m_spinlock_registry_lock;
        std::vector<SpinLockImpl *> m_spinlocks;

        std::unique_ptr<DeadlockDetector> m_deadlock_detector;
    };
} // namespace BML::Core

#endif // BML_CORE_SYNC_MANAGER_H
