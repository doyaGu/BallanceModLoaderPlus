#include "SyncManager.h"

#include <algorithm>
#include <functional>
#include <new>
#include <unordered_map>
#include <unordered_set>

#include "CoreErrors.h"
#include "DiagnosticManager.h"

namespace BML::Core {
    class DeadlockDetector {
    public:
        bool OnLockWait(void *lock_key, DWORD thread_id) {
            std::lock_guard<std::mutex> guard(m_mutex);
            ThreadState &thread_state = m_threads[thread_id];
            thread_state.waiting_lock = lock_key;
            std::unordered_set<void *> lock_visited;
            std::unordered_set<DWORD> thread_visited;
            bool has_cycle = DetectCycle(lock_key, thread_id, lock_visited, thread_visited);
            if (has_cycle) {
                thread_state.waiting_lock = nullptr;
            }
            return has_cycle;
        }

        void OnLockAcquired(void *lock_key, DWORD thread_id) {
            std::lock_guard<std::mutex> guard(m_mutex);
            ThreadState &thread_state = m_threads[thread_id];
            thread_state.waiting_lock = nullptr;
            thread_state.held_locks[lock_key]++;

            LockState &lock_state = m_locks[lock_key];
            lock_state.owners[thread_id]++;
        }

        void OnLockWaitCancelled(void *lock_key, DWORD thread_id) {
            std::lock_guard<std::mutex> guard(m_mutex);
            auto thread_it = m_threads.find(thread_id);
            if (thread_it == m_threads.end()) {
                return;
            }

            auto &thread_state = thread_it->second;
            if (thread_state.waiting_lock == lock_key) {
                thread_state.waiting_lock = nullptr;
                if (thread_state.held_locks.empty()) {
                    m_threads.erase(thread_it);
                }
            }
        }

        void OnLockReleased(void *lock_key, DWORD thread_id) {
            std::lock_guard<std::mutex> guard(m_mutex);
            auto thread_it = m_threads.find(thread_id);
            if (thread_it != m_threads.end()) {
                auto &thread_state = thread_it->second;
                auto held_it = thread_state.held_locks.find(lock_key);
                if (held_it != thread_state.held_locks.end()) {
                    if (--held_it->second == 0) {
                        thread_state.held_locks.erase(held_it);
                    }
                }
                if (thread_state.held_locks.empty() && thread_state.waiting_lock == nullptr) {
                    m_threads.erase(thread_it);
                }
            }

            auto lock_it = m_locks.find(lock_key);
            if (lock_it != m_locks.end()) {
                auto &lock_state = lock_it->second;
                auto owner_it = lock_state.owners.find(thread_id);
                if (owner_it != lock_state.owners.end()) {
                    if (--owner_it->second == 0) {
                        lock_state.owners.erase(owner_it);
                    }
                }
                if (lock_state.owners.empty()) {
                    m_locks.erase(lock_it);
                }
            }
        }

    private:
        struct ThreadState {
            void *waiting_lock{nullptr};
            std::unordered_map<void *, uint32_t> held_locks;
        };

        struct LockState {
            std::unordered_map<DWORD, uint32_t> owners;
        };

        bool DetectCycle(void *lock_key,
                         DWORD target_thread,
                         std::unordered_set<void *> &lock_visited,
                         std::unordered_set<DWORD> &thread_visited) {
            if (!lock_key) {
                return false;
            }

            if (!lock_visited.insert(lock_key).second) {
                return false;
            }

            auto lock_it = m_locks.find(lock_key);
            if (lock_it == m_locks.end()) {
                return false;
            }

            for (const auto &owner_pair : lock_it->second.owners) {
                DWORD owner_thread = owner_pair.first;
                if (owner_thread == target_thread) {
                    return true;
                }

                if (!thread_visited.insert(owner_thread).second) {
                    continue;
                }

                auto thread_it = m_threads.find(owner_thread);
                if (thread_it == m_threads.end()) {
                    continue;
                }

                void *waiting_lock = thread_it->second.waiting_lock;
                if (waiting_lock && DetectCycle(waiting_lock, target_thread, lock_visited, thread_visited)) {
                    return true;
                }
            }
            return false;
        }

        std::mutex m_mutex;
        std::unordered_map<DWORD, ThreadState> m_threads;
        std::unordered_map<void *, LockState> m_locks;
    };

    namespace {
        class WaitRegistration {
        public:
            using Reporter = std::function<BML_Result()>;

            WaitRegistration(DeadlockDetector *detector,
                             void *lock_key,
                             DWORD thread_id,
                             Reporter reporter)
                : m_detector(detector),
                  m_lock_key(lock_key),
                  m_thread_id(thread_id),
                  m_reporter(std::move(reporter)) {}

            BML_Result Ensure() {
                if (m_registered || !m_detector) {
                    return BML_RESULT_OK;
                }
                if (m_detector->OnLockWait(m_lock_key, m_thread_id)) {
                    return m_reporter ? m_reporter() : BML_RESULT_SYNC_DEADLOCK;
                }
                m_registered = true;
                return BML_RESULT_OK;
            }

            void Cancel() {
                if (!m_registered || !m_detector) {
                    return;
                }
                m_detector->OnLockWaitCancelled(m_lock_key, m_thread_id);
                m_registered = false;
            }

            void MarkAcquired() {
                m_registered = false;
            }

        private:
            DeadlockDetector *m_detector{};
            void *m_lock_key{};
            DWORD m_thread_id{0};
            Reporter m_reporter;
            bool m_registered{false};
        };
    } // namespace

    SyncManager::SyncManager() : m_DeadlockDetector(std::make_unique<DeadlockDetector>()) {
    }

    namespace {
        inline BML_Result ReportInvalidSyncCall(const char *api_name, const char *message) {
            return SetLastErrorAndReturn(BML_RESULT_INVALID_ARGUMENT, "sync", api_name, message, 0);
        }
    } // namespace

    SyncManager &SyncManager::Instance() {
        static SyncManager instance;
        return instance;
    }

    // ============================================================================
    // Mutex Operations
    // ============================================================================

    BML_Result SyncManager::CreateMutex(BML_Mutex *out_mutex) {
        if (!out_mutex) {
            return SetLastErrorAndReturn(BML_RESULT_INVALID_ARGUMENT, "sync", "bmlMutexCreate",
                                         "out_mutex is NULL", 0);
        }

        try {
            auto *impl = new MutexImpl();

            std::lock_guard<std::mutex> lock(m_MutexRegistryLock);
            m_Mutexes.push_back(impl);

            *out_mutex = reinterpret_cast<BML_Mutex>(impl);
            return BML_RESULT_OK;
        } catch (const std::bad_alloc &) {
            return SetLastErrorAndReturn(BML_RESULT_OUT_OF_MEMORY, "sync", "bmlMutexCreate",
                                         "Failed to allocate mutex", 0);
        } catch (...) {
            return SetLastErrorAndReturn(BML_RESULT_UNKNOWN_ERROR, "sync", "bmlMutexCreate",
                                         "Unknown error creating mutex", 0);
        }
    }

    void SyncManager::DestroyMutex(BML_Mutex mutex) {
        if (!mutex) {
            ReportInvalidSyncCall("bmlMutexDestroy", "mutex handle is NULL");
            return;
        }

        auto *impl = reinterpret_cast<MutexImpl *>(mutex);

        std::lock_guard<std::mutex> lock(m_MutexRegistryLock);
        auto it = std::find(m_Mutexes.begin(), m_Mutexes.end(), impl);
        if (it == m_Mutexes.end()) {
            ReportInvalidSyncCall("bmlMutexDestroy", "mutex handle is invalid or already destroyed");
            return;
        }

        delete impl;
        m_Mutexes.erase(it);
    }

    void SyncManager::LockMutex(BML_Mutex mutex) {
        if (!ValidateMutexHandle(mutex, "bmlMutexLock")) {
            return;
        }

        auto *impl = reinterpret_cast<MutexImpl *>(mutex);
        DWORD thread_id = ::GetCurrentThreadId();

        if (TryEnterCriticalSection(&impl->cs)) {
            m_DeadlockDetector->OnLockAcquired(impl, thread_id);
            return;
        }

        if (m_DeadlockDetector->OnLockWait(impl, thread_id)) {
            ReportDeadlock("bmlMutexLock");
            return;
        }

        EnterCriticalSection(&impl->cs);
        m_DeadlockDetector->OnLockAcquired(impl, thread_id);
    }

    BML_Bool SyncManager::TryLockMutex(BML_Mutex mutex) {
        if (!ValidateMutexHandle(mutex, "bmlMutexTryLock")) {
            return BML_FALSE;
        }

        auto *impl = reinterpret_cast<MutexImpl *>(mutex);
        DWORD thread_id = ::GetCurrentThreadId();
        if (TryEnterCriticalSection(&impl->cs)) {
            m_DeadlockDetector->OnLockAcquired(impl, thread_id);
            return BML_TRUE;
        }
        return BML_FALSE;
    }

    void SyncManager::UnlockMutex(BML_Mutex mutex) {
        if (!ValidateMutexHandle(mutex, "bmlMutexUnlock")) {
            return;
        }

        auto *impl = reinterpret_cast<MutexImpl *>(mutex);
        DWORD thread_id = ::GetCurrentThreadId();
        m_DeadlockDetector->OnLockReleased(impl, thread_id);
        LeaveCriticalSection(&impl->cs);
    }

    BML_Result SyncManager::LockMutexTimeout(BML_Mutex mutex, uint32_t timeout_ms) {
        if (!ValidateMutexHandle(mutex, "bmlMutexLockTimeout")) {
            return BML_RESULT_INVALID_HANDLE;
        }

        auto *impl = reinterpret_cast<MutexImpl *>(mutex);
        DWORD thread_id = ::GetCurrentThreadId();

        WaitRegistration wait_reg(
            m_DeadlockDetector.get(),
            impl,
            thread_id,
            [this]() { return ReportDeadlock("bmlMutexLockTimeout"); });

        // Handle special timeout values
        if (timeout_ms == BML_TIMEOUT_INFINITE) {
            if (TryEnterCriticalSection(&impl->cs)) {
                m_DeadlockDetector->OnLockAcquired(impl, thread_id);
                return BML_RESULT_OK;
            }

            auto status = wait_reg.Ensure();
            if (status != BML_RESULT_OK) {
                return status;
            }

            EnterCriticalSection(&impl->cs);
            wait_reg.MarkAcquired();
            m_DeadlockDetector->OnLockAcquired(impl, thread_id);
            return BML_RESULT_OK;
        }

        if (timeout_ms == BML_TIMEOUT_NONE) {
            // Non-blocking try
            if (TryEnterCriticalSection(&impl->cs)) {
                m_DeadlockDetector->OnLockAcquired(impl, thread_id);
                return BML_RESULT_OK;
            }
            return BML_RESULT_TIMEOUT;
        }

        // Spin with timeout using high-resolution timer
        LARGE_INTEGER freq, start, now;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&start);

        double timeout_sec = timeout_ms / 1000.0;

        while (true) {
            if (TryEnterCriticalSection(&impl->cs)) {
                wait_reg.MarkAcquired();
                m_DeadlockDetector->OnLockAcquired(impl, thread_id);
                return BML_RESULT_OK;
            }

            auto status = wait_reg.Ensure();
            if (status != BML_RESULT_OK) {
                return status;
            }

            QueryPerformanceCounter(&now);
            double elapsed = static_cast<double>(now.QuadPart - start.QuadPart) / freq.QuadPart;
            if (elapsed >= timeout_sec) {
                wait_reg.Cancel();
                return BML_RESULT_TIMEOUT;
            }

            // Yield to avoid busy-spinning
            ::SwitchToThread();
        }
    }

    bool SyncManager::IsValidMutex(BML_Mutex mutex) const {
        if (!mutex) {
            return false;
        }

        auto *impl = reinterpret_cast<MutexImpl *>(mutex);
        std::lock_guard<std::mutex> lock(const_cast<std::mutex &>(m_MutexRegistryLock));
        return std::find(m_Mutexes.begin(), m_Mutexes.end(), impl) != m_Mutexes.end();
    }

    // ============================================================================
    // RwLock Operations
    // ============================================================================

    BML_Result SyncManager::CreateRwLock(BML_RwLock *out_lock) {
        if (!out_lock) {
            return SetLastErrorAndReturn(BML_RESULT_INVALID_ARGUMENT, "sync", "bmlRwLockCreate",
                                         "out_lock is NULL", 0);
        }

        try {
            auto *impl = new RwLockImpl();

            std::lock_guard<std::mutex> lock(m_RWLockRegistryLock);
            m_RWLocks.push_back(impl);

            *out_lock = reinterpret_cast<BML_RwLock>(impl);
            return BML_RESULT_OK;
        } catch (const std::bad_alloc &) {
            return SetLastErrorAndReturn(BML_RESULT_OUT_OF_MEMORY, "sync", "bmlRwLockCreate",
                                         "Failed to allocate read-write lock", 0);
        } catch (...) {
            return SetLastErrorAndReturn(BML_RESULT_UNKNOWN_ERROR, "sync", "bmlRwLockCreate",
                                         "Unknown error creating read-write lock", 0);
        }
    }

    void SyncManager::DestroyRwLock(BML_RwLock lock) {
        if (!lock) {
            ReportInvalidSyncCall("bmlRwLockDestroy", "rwlock handle is NULL");
            return;
        }

        auto *impl = reinterpret_cast<RwLockImpl *>(lock);

        std::lock_guard<std::mutex> registry_lock(m_RWLockRegistryLock);
        auto it = std::find(m_RWLocks.begin(), m_RWLocks.end(), impl);
        if (it == m_RWLocks.end()) {
            ReportInvalidSyncCall("bmlRwLockDestroy", "rwlock handle is invalid or already destroyed");
            return;
        }

        delete impl;
        m_RWLocks.erase(it);
    }

    void SyncManager::ReadLockRwLock(BML_RwLock lock) {
        if (!ValidateRwLockHandle(lock, "bmlRwLockReadLock")) {
            return;
        }

        auto *impl = reinterpret_cast<RwLockImpl *>(lock);
        auto state = impl->GetThreadState();
        DWORD thread_id = ::GetCurrentThreadId();

        if (state.write_depth > 0) {
            ReportLockMisuse("bmlRwLockReadLock", "Cannot acquire read lock while holding write lock in the same thread");
            return;
        }

        if (state.read_depth == 0) {
            if (TryAcquireSRWLockShared(&impl->srw)) {
                m_DeadlockDetector->OnLockAcquired(impl, thread_id);
            } else {
                if (m_DeadlockDetector->OnLockWait(impl, thread_id)) {
                    ReportDeadlock("bmlRwLockReadLock");
                    return;
                }
                AcquireSRWLockShared(&impl->srw);
                m_DeadlockDetector->OnLockAcquired(impl, thread_id);
            }
        } else if (state.read_depth == RwLockImpl::kMaxRecursionDepth) {
            ReportLockMisuse("bmlRwLockReadLock", "Read lock recursion depth exceeded");
            return;
        }

        state.read_depth++;
        impl->SetThreadState(state);
    }

    BML_Bool SyncManager::TryReadLockRwLock(BML_RwLock lock) {
        if (!ValidateRwLockHandle(lock, "bmlRwLockTryReadLock")) {
            return BML_FALSE;
        }

        auto *impl = reinterpret_cast<RwLockImpl *>(lock);
        auto state = impl->GetThreadState();
        DWORD thread_id = ::GetCurrentThreadId();

        if (state.write_depth > 0) {
            ReportLockMisuse("bmlRwLockTryReadLock", "Cannot acquire read lock while holding write lock in the same thread");
            return BML_FALSE;
        }

        if (state.read_depth > 0) {
            if (state.read_depth == RwLockImpl::kMaxRecursionDepth) {
                ReportLockMisuse("bmlRwLockTryReadLock", "Read lock recursion depth exceeded");
                return BML_FALSE;
            }
            state.read_depth++;
            impl->SetThreadState(state);
            return BML_TRUE;
        }

        if (TryAcquireSRWLockShared(&impl->srw)) {
            state.read_depth = 1;
            impl->SetThreadState(state);
            m_DeadlockDetector->OnLockAcquired(impl, thread_id);
            return BML_TRUE;
        }
        return BML_FALSE;
    }

    BML_Result SyncManager::ReadLockRwLockTimeout(BML_RwLock lock, uint32_t timeout_ms) {
        if (!ValidateRwLockHandle(lock, "bmlRwLockReadLockTimeout")) {
            return BML_RESULT_INVALID_HANDLE;
        }

        auto *impl = reinterpret_cast<RwLockImpl *>(lock);
        auto state = impl->GetThreadState();
        DWORD thread_id = ::GetCurrentThreadId();

        if (state.write_depth > 0) {
            return SetLastErrorAndReturn(BML_RESULT_SYNC_DEADLOCK, "sync", "bmlRwLockReadLockTimeout",
                                         "Cannot acquire read lock while holding write lock in the same thread", 0);
        }

        // If already holding read lock, increment depth (recursive)
        if (state.read_depth > 0) {
            if (state.read_depth == RwLockImpl::kMaxRecursionDepth) {
                return SetLastErrorAndReturn(BML_RESULT_FAIL, "sync", "bmlRwLockReadLockTimeout",
                                             "Read lock recursion depth exceeded", 0);
            }
            state.read_depth++;
            impl->SetThreadState(state);
            return BML_RESULT_OK;
        }

        // Handle special timeout values
        WaitRegistration wait_reg(
            m_DeadlockDetector.get(),
            impl,
            thread_id,
            [this]() { return ReportDeadlock("bmlRwLockReadLockTimeout"); });

        if (timeout_ms == BML_TIMEOUT_INFINITE) {
            if (TryAcquireSRWLockShared(&impl->srw)) {
                state.read_depth = 1;
                impl->SetThreadState(state);
                m_DeadlockDetector->OnLockAcquired(impl, thread_id);
                return BML_RESULT_OK;
            }

            auto status = wait_reg.Ensure();
            if (status != BML_RESULT_OK) {
                return status;
            }

            AcquireSRWLockShared(&impl->srw);
            wait_reg.MarkAcquired();
            m_DeadlockDetector->OnLockAcquired(impl, thread_id);
            state.read_depth = 1;
            impl->SetThreadState(state);
            return BML_RESULT_OK;
        }

        if (timeout_ms == BML_TIMEOUT_NONE) {
            if (TryAcquireSRWLockShared(&impl->srw)) {
                state.read_depth = 1;
                impl->SetThreadState(state);
                m_DeadlockDetector->OnLockAcquired(impl, thread_id);
                return BML_RESULT_OK;
            }
            return BML_RESULT_TIMEOUT;
        }

        // Spin with timeout
        LARGE_INTEGER freq, start, now;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&start);
        double timeout_sec = timeout_ms / 1000.0;

        while (true) {
            if (TryAcquireSRWLockShared(&impl->srw)) {
                wait_reg.MarkAcquired();
                state.read_depth = 1;
                impl->SetThreadState(state);
                m_DeadlockDetector->OnLockAcquired(impl, thread_id);
                return BML_RESULT_OK;
            }

            auto status = wait_reg.Ensure();
            if (status != BML_RESULT_OK) {
                return status;
            }

            QueryPerformanceCounter(&now);
            double elapsed = static_cast<double>(now.QuadPart - start.QuadPart) / freq.QuadPart;
            if (elapsed >= timeout_sec) {
                wait_reg.Cancel();
                return BML_RESULT_TIMEOUT;
            }

            ::SwitchToThread();
        }
    }

    void SyncManager::WriteLockRwLock(BML_RwLock lock) {
        if (!ValidateRwLockHandle(lock, "bmlRwLockWriteLock")) {
            return;
        }

        auto *impl = reinterpret_cast<RwLockImpl *>(lock);
        auto state = impl->GetThreadState();
        DWORD thread_id = ::GetCurrentThreadId();

        if (state.write_depth > 0) {
            if (state.write_depth == RwLockImpl::kMaxRecursionDepth) {
                ReportLockMisuse("bmlRwLockWriteLock", "Write lock recursion depth exceeded");
                return;
            }
            state.write_depth++;
            impl->SetThreadState(state);
            return;
        }

        if (state.read_depth > 0) {
            ReportLockMisuse("bmlRwLockWriteLock", "Cannot upgrade read lock to write lock without unlocking");
            return;
        }

        if (TryAcquireSRWLockExclusive(&impl->srw)) {
            m_DeadlockDetector->OnLockAcquired(impl, thread_id);
        } else {
            if (m_DeadlockDetector->OnLockWait(impl, thread_id)) {
                ReportDeadlock("bmlRwLockWriteLock");
                return;
            }
            AcquireSRWLockExclusive(&impl->srw);
            m_DeadlockDetector->OnLockAcquired(impl, thread_id);
        }
        state.write_depth = 1;
        impl->SetThreadState(state);
    }

    BML_Bool SyncManager::TryWriteLockRwLock(BML_RwLock lock) {
        if (!ValidateRwLockHandle(lock, "bmlRwLockTryWriteLock")) {
            return BML_FALSE;
        }

        auto *impl = reinterpret_cast<RwLockImpl *>(lock);
        auto state = impl->GetThreadState();
        DWORD thread_id = ::GetCurrentThreadId();

        if (state.write_depth > 0) {
            if (state.write_depth == RwLockImpl::kMaxRecursionDepth) {
                ReportLockMisuse("bmlRwLockTryWriteLock", "Write lock recursion depth exceeded");
                return BML_FALSE;
            }
            state.write_depth++;
            impl->SetThreadState(state);
            return BML_TRUE;
        }

        if (state.read_depth > 0) {
            ReportLockMisuse("bmlRwLockTryWriteLock", "Cannot upgrade read lock to write lock without unlocking");
            return BML_FALSE;
        }

        if (TryAcquireSRWLockExclusive(&impl->srw)) {
            state.write_depth = 1;
            impl->SetThreadState(state);
            m_DeadlockDetector->OnLockAcquired(impl, thread_id);
            return BML_TRUE;
        }
        return BML_FALSE;
    }

    BML_Result SyncManager::WriteLockRwLockTimeout(BML_RwLock lock, uint32_t timeout_ms) {
        if (!ValidateRwLockHandle(lock, "bmlRwLockWriteLockTimeout")) {
            return BML_RESULT_INVALID_HANDLE;
        }

        auto *impl = reinterpret_cast<RwLockImpl *>(lock);
        auto state = impl->GetThreadState();
        DWORD thread_id = ::GetCurrentThreadId();

        // If already holding write lock, increment depth (recursive)
        if (state.write_depth > 0) {
            if (state.write_depth == RwLockImpl::kMaxRecursionDepth) {
                return SetLastErrorAndReturn(BML_RESULT_FAIL, "sync", "bmlRwLockWriteLockTimeout",
                                             "Write lock recursion depth exceeded", 0);
            }
            state.write_depth++;
            impl->SetThreadState(state);
            return BML_RESULT_OK;
        }

        if (state.read_depth > 0) {
            return SetLastErrorAndReturn(BML_RESULT_SYNC_DEADLOCK, "sync", "bmlRwLockWriteLockTimeout",
                                         "Cannot upgrade read lock to write lock without unlocking", 0);
        }

        // Handle special timeout values
        WaitRegistration wait_reg(
            m_DeadlockDetector.get(),
            impl,
            thread_id,
            [this]() { return ReportDeadlock("bmlRwLockWriteLockTimeout"); });

        if (timeout_ms == BML_TIMEOUT_INFINITE) {
            if (TryAcquireSRWLockExclusive(&impl->srw)) {
                state.write_depth = 1;
                impl->SetThreadState(state);
                m_DeadlockDetector->OnLockAcquired(impl, thread_id);
                return BML_RESULT_OK;
            }

            auto status = wait_reg.Ensure();
            if (status != BML_RESULT_OK) {
                return status;
            }

            AcquireSRWLockExclusive(&impl->srw);
            wait_reg.MarkAcquired();
            m_DeadlockDetector->OnLockAcquired(impl, thread_id);
            state.write_depth = 1;
            impl->SetThreadState(state);
            return BML_RESULT_OK;
        }

        if (timeout_ms == BML_TIMEOUT_NONE) {
            if (TryAcquireSRWLockExclusive(&impl->srw)) {
                state.write_depth = 1;
                impl->SetThreadState(state);
                m_DeadlockDetector->OnLockAcquired(impl, thread_id);
                return BML_RESULT_OK;
            }
            return BML_RESULT_TIMEOUT;
        }

        // Spin with timeout
        LARGE_INTEGER freq, start, now;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&start);
        double timeout_sec = timeout_ms / 1000.0;

        while (true) {
            if (TryAcquireSRWLockExclusive(&impl->srw)) {
                wait_reg.MarkAcquired();
                state.write_depth = 1;
                impl->SetThreadState(state);
                m_DeadlockDetector->OnLockAcquired(impl, thread_id);
                return BML_RESULT_OK;
            }

            auto status = wait_reg.Ensure();
            if (status != BML_RESULT_OK) {
                return status;
            }

            QueryPerformanceCounter(&now);
            double elapsed = static_cast<double>(now.QuadPart - start.QuadPart) / freq.QuadPart;
            if (elapsed >= timeout_sec) {
                wait_reg.Cancel();
                return BML_RESULT_TIMEOUT;
            }

            ::SwitchToThread();
        }
    }

    void SyncManager::UnlockRwLock(BML_RwLock lock) {
        if (!ValidateRwLockHandle(lock, "bmlRwLockUnlock")) {
            return;
        }

        auto *impl = reinterpret_cast<RwLockImpl *>(lock);
        auto state = impl->GetThreadState();
        DWORD thread_id = ::GetCurrentThreadId();

        if (state.write_depth > 0) {
            state.write_depth--;
            if (state.write_depth == 0) {
                ReleaseSRWLockExclusive(&impl->srw);
                m_DeadlockDetector->OnLockReleased(impl, thread_id);
            }
            impl->SetThreadState(state);
            return;
        }

        if (state.read_depth > 0) {
            state.read_depth--;
            if (state.read_depth == 0) {
                ReleaseSRWLockShared(&impl->srw);
                m_DeadlockDetector->OnLockReleased(impl, thread_id);
            }
            impl->SetThreadState(state);
            return;
        }

        ReportLockMisuse("bmlRwLockUnlock", "Unlock called without a matching lock");
    }

    void SyncManager::ReadUnlockRwLock(BML_RwLock lock) {
        if (!ValidateRwLockHandle(lock, "bmlRwLockReadUnlock")) {
            return;
        }

        auto *impl = reinterpret_cast<RwLockImpl *>(lock);
        auto state = impl->GetThreadState();
        DWORD thread_id = ::GetCurrentThreadId();

        if (state.write_depth > 0) {
            ReportLockMisuse("bmlRwLockReadUnlock", "Cannot perform read unlock while holding a write lock");
            return;
        }

        if (state.read_depth == 0) {
            ReportLockMisuse("bmlRwLockReadUnlock", "Read unlock called without a matching lock");
            return;
        }

        state.read_depth--;
        if (state.read_depth == 0) {
            ReleaseSRWLockShared(&impl->srw);
            m_DeadlockDetector->OnLockReleased(impl, thread_id);
        }
        impl->SetThreadState(state);
    }

    void SyncManager::WriteUnlockRwLock(BML_RwLock lock) {
        if (!ValidateRwLockHandle(lock, "bmlRwLockWriteUnlock")) {
            return;
        }

        auto *impl = reinterpret_cast<RwLockImpl *>(lock);
        auto state = impl->GetThreadState();
        DWORD thread_id = ::GetCurrentThreadId();

        if (state.write_depth == 0) {
            ReportLockMisuse("bmlRwLockWriteUnlock", "Write unlock called without a matching lock");
            return;
        }

        state.write_depth--;
        if (state.write_depth == 0) {
            ReleaseSRWLockExclusive(&impl->srw);
            m_DeadlockDetector->OnLockReleased(impl, thread_id);
        }
        impl->SetThreadState(state);
    }

    bool SyncManager::IsValidRwLock(BML_RwLock lock) const {
        if (!lock) {
            return false;
        }

        auto *impl = reinterpret_cast<RwLockImpl *>(lock);
        std::lock_guard<std::mutex> registry_lock(const_cast<std::mutex &>(m_RWLockRegistryLock));
        return std::find(m_RWLocks.begin(), m_RWLocks.end(), impl) != m_RWLocks.end();
    }

    // ============================================================================
    // Atomic Operations (inlined Windows Interlocked functions)
    // ============================================================================

    int32_t SyncManager::AtomicIncrement32(volatile int32_t *value) {
        return InterlockedIncrement(reinterpret_cast<volatile LONG *>(value));
    }

    int32_t SyncManager::AtomicDecrement32(volatile int32_t *value) {
        return InterlockedDecrement(reinterpret_cast<volatile LONG *>(value));
    }

    int32_t SyncManager::AtomicAdd32(volatile int32_t *value, int32_t addend) {
        return InterlockedExchangeAdd(reinterpret_cast<volatile LONG *>(value), addend);
    }

    int32_t SyncManager::AtomicCompareExchange32(volatile int32_t *dest, int32_t exchange, int32_t comparand) {
        return InterlockedCompareExchange(
            reinterpret_cast<volatile LONG *>(dest),
            exchange,
            comparand
        );
    }

    int32_t SyncManager::AtomicExchange32(volatile int32_t *dest, int32_t new_value) {
        return InterlockedExchange(reinterpret_cast<volatile LONG *>(dest), new_value);
    }

    void *SyncManager::AtomicLoadPtr(void *volatile*ptr) {
        // On x86/x64, pointer reads are atomic
        // Use MemoryBarrier for strict ordering
        void *value = *ptr;
        MemoryBarrier();
        return value;
    }

    void SyncManager::AtomicStorePtr(void *volatile*ptr, void *value) {
        // Use InterlockedExchangePointer for atomic store
        InterlockedExchangePointer(ptr, value);
    }

    void *SyncManager::AtomicCompareExchangePtr(void *volatile*dest, void *exchange, void *comparand) {
        return InterlockedCompareExchangePointer(dest, exchange, comparand);
    }

    // ============================================================================
    // Semaphore Operations
    // ============================================================================

    BML_Result SyncManager::CreateSemaphore(uint32_t initial_count, uint32_t max_count, BML_Semaphore *out_semaphore) {
        if (!out_semaphore) {
            return SetLastErrorAndReturn(BML_RESULT_INVALID_ARGUMENT, "sync", "bmlSemaphoreCreate",
                                         "out_semaphore is NULL", 0);
        }

        if (initial_count > max_count) {
            return SetLastErrorAndReturn(BML_RESULT_INVALID_ARGUMENT, "sync", "bmlSemaphoreCreate",
                                         "initial_count > max_count", 0);
        }

        try {
            auto *impl = new SemaphoreImpl(initial_count, max_count);

            if (!impl->handle) {
                delete impl;
                return SetLastErrorAndReturn(BML_RESULT_UNKNOWN_ERROR, "sync", "bmlSemaphoreCreate",
                                             "CreateSemaphore failed", ::GetLastError());
            }

            std::lock_guard<std::mutex> lock(m_SemaphoreRegistryLock);
            m_Semaphores.push_back(impl);

            *out_semaphore = reinterpret_cast<BML_Semaphore>(impl);
            return BML_RESULT_OK;
        } catch (const std::bad_alloc &) {
            return SetLastErrorAndReturn(BML_RESULT_OUT_OF_MEMORY, "sync", "bmlSemaphoreCreate",
                                         "Failed to allocate semaphore", 0);
        } catch (...) {
            return SetLastErrorAndReturn(BML_RESULT_UNKNOWN_ERROR, "sync", "bmlSemaphoreCreate",
                                         "Unknown error creating semaphore", 0);
        }
    }

    void SyncManager::DestroySemaphore(BML_Semaphore semaphore) {
        if (!semaphore) {
            ReportInvalidSyncCall("bmlSemaphoreDestroy", "semaphore handle is NULL");
            return;
        }

        auto *impl = reinterpret_cast<SemaphoreImpl *>(semaphore);

        std::lock_guard<std::mutex> lock(m_SemaphoreRegistryLock);
        auto it = std::find(m_Semaphores.begin(), m_Semaphores.end(), impl);
        if (it == m_Semaphores.end()) {
            ReportInvalidSyncCall("bmlSemaphoreDestroy", "semaphore handle is invalid or already destroyed");
            return;
        }

        delete impl;
        m_Semaphores.erase(it);
    }

    BML_Result SyncManager::WaitSemaphore(BML_Semaphore semaphore, uint32_t timeout_ms) {
        if (!ValidateSemaphoreHandle(semaphore, "bmlSemaphoreWait")) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        auto *impl = reinterpret_cast<SemaphoreImpl *>(semaphore);
        DWORD thread_id = ::GetCurrentThreadId();

        WaitRegistration wait_reg(
            m_DeadlockDetector.get(),
            impl,
            thread_id,
            [this]() { return ReportDeadlock("bmlSemaphoreWait"); });

        auto handle_acquired = [&]() {
            wait_reg.MarkAcquired();
            m_DeadlockDetector->OnLockAcquired(impl, thread_id);
        };

        auto immediate_result = WaitForSingleObject(impl->handle, 0);
        if (immediate_result == WAIT_OBJECT_0) {
            handle_acquired();
            return BML_RESULT_OK;
        }
        if (immediate_result == WAIT_FAILED) {
            return SetLastErrorAndReturn(BML_RESULT_UNKNOWN_ERROR, "sync", "bmlSemaphoreWait",
                                         "WaitForSingleObject failed", ::GetLastError());
        }
        if (timeout_ms == BML_TIMEOUT_NONE) {
            return BML_RESULT_TIMEOUT;
        }

        BML_Result wait_status = wait_reg.Ensure();
        if (wait_status != BML_RESULT_OK) {
            return wait_status;
        }

        DWORD wait_duration = (timeout_ms == BML_TIMEOUT_INFINITE) ? INFINITE : timeout_ms;
        DWORD result = WaitForSingleObject(impl->handle, wait_duration);

        switch (result) {
        case WAIT_OBJECT_0:
            handle_acquired();
            return BML_RESULT_OK;
        case WAIT_TIMEOUT:
            wait_reg.Cancel();
            return BML_RESULT_TIMEOUT;
        case WAIT_FAILED:
            wait_reg.Cancel();
            return SetLastErrorAndReturn(BML_RESULT_UNKNOWN_ERROR, "sync", "bmlSemaphoreWait",
                                         "WaitForSingleObject failed", ::GetLastError());
        default:
            wait_reg.Cancel();
            return SetLastErrorAndReturn(BML_RESULT_UNKNOWN_ERROR, "sync", "bmlSemaphoreWait",
                                         "Unexpected wait result", result);
        }
    }

    BML_Result SyncManager::SignalSemaphore(BML_Semaphore semaphore, uint32_t count) {
        if (!ValidateSemaphoreHandle(semaphore, "bmlSemaphoreSignal")) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        if (count == 0) {
            return BML_RESULT_OK; // No-op
        }

        auto *impl = reinterpret_cast<SemaphoreImpl *>(semaphore);

        if (!ReleaseSemaphore(impl->handle, count, nullptr)) {
            return SetLastErrorAndReturn(BML_RESULT_UNKNOWN_ERROR, "sync", "bmlSemaphoreSignal",
                                         "ReleaseSemaphore failed", ::GetLastError());
        }

        DWORD thread_id = ::GetCurrentThreadId();
        for (uint32_t i = 0; i < count; ++i) {
            m_DeadlockDetector->OnLockReleased(impl, thread_id);
        }

        return BML_RESULT_OK;
    }

    bool SyncManager::IsValidSemaphore(BML_Semaphore semaphore) const {
        if (!semaphore) {
            return false;
        }

        auto *impl = reinterpret_cast<SemaphoreImpl *>(semaphore);
        std::lock_guard<std::mutex> lock(const_cast<std::mutex &>(m_SemaphoreRegistryLock));
        return std::find(m_Semaphores.begin(), m_Semaphores.end(), impl) != m_Semaphores.end();
    }

    // ============================================================================
    // TLS Operations
    // ============================================================================

    BML_Result SyncManager::CreateTls(BML_TlsDestructor destructor, BML_TlsKey *out_key) {
        if (!out_key) {
            return SetLastErrorAndReturn(BML_RESULT_INVALID_ARGUMENT, "sync", "bmlTlsCreate",
                                         "out_key is NULL", 0);
        }

        try {
            auto *impl = new TlsKeyImpl(destructor);

            if (impl->fls_index == FLS_OUT_OF_INDEXES) {
                delete impl;
                return SetLastErrorAndReturn(BML_RESULT_UNKNOWN_ERROR, "sync", "bmlTlsCreate",
                                             "FlsAlloc failed", ::GetLastError());
            }

            std::lock_guard<std::mutex> lock(m_TLSRegistryLock);
            m_TLSKeys.push_back(impl);

            *out_key = reinterpret_cast<BML_TlsKey>(impl);
            return BML_RESULT_OK;
        } catch (const std::bad_alloc &) {
            return SetLastErrorAndReturn(BML_RESULT_OUT_OF_MEMORY, "sync", "bmlTlsCreate",
                                         "Failed to allocate TLS key", 0);
        } catch (...) {
            return SetLastErrorAndReturn(BML_RESULT_UNKNOWN_ERROR, "sync", "bmlTlsCreate",
                                         "Unknown error creating TLS key", 0);
        }
    }

    void SyncManager::DestroyTls(BML_TlsKey key) {
        if (!key) {
            ReportInvalidSyncCall("bmlTlsDestroy", "TLS key is NULL");
            return;
        }

        auto *impl = reinterpret_cast<TlsKeyImpl *>(key);

        std::lock_guard<std::mutex> lock(m_TLSRegistryLock);
        auto it = std::find(m_TLSKeys.begin(), m_TLSKeys.end(), impl);
        if (it == m_TLSKeys.end()) {
            ReportInvalidSyncCall("bmlTlsDestroy", "TLS key is invalid or already destroyed");
            return;
        }

        delete impl;
        m_TLSKeys.erase(it);
    }

    void *SyncManager::GetTls(BML_TlsKey key) {
        if (!ValidateTlsHandle(key, "bmlTlsGet")) {
            return nullptr;
        }

        auto *impl = reinterpret_cast<TlsKeyImpl *>(key);
        void *stored = FlsGetValue(impl->fls_index);
        if (!stored) {
            return nullptr;
        }
        // If destructor is set, we stored a wrapper
        if (impl->destructor) {
            auto *wrapper = static_cast<TlsValueWrapper *>(stored);
            return wrapper->value;
        }
        return stored;
    }

    BML_Result SyncManager::SetTls(BML_TlsKey key, void *value) {
        if (!ValidateTlsHandle(key, "bmlTlsSet")) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        auto *impl = reinterpret_cast<TlsKeyImpl *>(key);

        // If destructor is set, use wrapper for proper cleanup on thread exit
        if (impl->destructor) {
            // Get existing wrapper if any
            void *existing = FlsGetValue(impl->fls_index);
            TlsValueWrapper *wrapper = nullptr;

            if (existing) {
                wrapper = static_cast<TlsValueWrapper *>(existing);
            }

            if (value == nullptr) {
                // Setting to null - free wrapper if exists
                if (wrapper) {
                    delete wrapper;
                    FlsSetValue(impl->fls_index, nullptr);
                }
                return BML_RESULT_OK;
            }

            // Create new wrapper or reuse existing
            if (!wrapper) {
                try {
                    wrapper = new TlsValueWrapper{value, impl->destructor};
                } catch (const std::bad_alloc &) {
                    return SetLastErrorAndReturn(BML_RESULT_OUT_OF_MEMORY, "sync", "bmlTlsSet",
                                                 "Failed to allocate TLS wrapper", 0);
                }
            } else {
                wrapper->value = value;
                wrapper->destructor = impl->destructor;
            }

            if (!FlsSetValue(impl->fls_index, wrapper)) {
                if (!existing) {
                    delete wrapper; // Only delete if we created it
                }
                return SetLastErrorAndReturn(BML_RESULT_UNKNOWN_ERROR, "sync", "bmlTlsSet",
                                             "FlsSetValue failed", ::GetLastError());
            }
        } else {
            // No destructor - store value directly
            if (!FlsSetValue(impl->fls_index, value)) {
                return SetLastErrorAndReturn(BML_RESULT_UNKNOWN_ERROR, "sync", "bmlTlsSet",
                                             "FlsSetValue failed", ::GetLastError());
            }
        }

        return BML_RESULT_OK;
    }

    bool SyncManager::IsValidTlsKey(BML_TlsKey key) const {
        if (!key) {
            return false;
        }

        auto *impl = reinterpret_cast<TlsKeyImpl *>(key);
        std::lock_guard<std::mutex> lock(const_cast<std::mutex &>(m_TLSRegistryLock));
        return std::find(m_TLSKeys.begin(), m_TLSKeys.end(), impl) != m_TLSKeys.end();
    }

    // ============================================================================
    // CondVar Operations
    // ============================================================================

    BML_Result SyncManager::CreateCondVar(BML_CondVar *out_condvar) {
        if (!out_condvar) {
            return SetLastErrorAndReturn(BML_RESULT_INVALID_ARGUMENT, "sync", "bmlCondVarCreate",
                                         "out_condvar is NULL", 0);
        }

        try {
            auto *impl = new CondVarImpl();

            std::lock_guard<std::mutex> lock(m_CondVarRegistryLock);
            m_CondVars.push_back(impl);

            *out_condvar = reinterpret_cast<BML_CondVar>(impl);
            return BML_RESULT_OK;
        } catch (const std::bad_alloc &) {
            return SetLastErrorAndReturn(BML_RESULT_OUT_OF_MEMORY, "sync", "bmlCondVarCreate",
                                         "Failed to allocate condition variable", 0);
        } catch (...) {
            return SetLastErrorAndReturn(BML_RESULT_UNKNOWN_ERROR, "sync", "bmlCondVarCreate",
                                         "Unknown error creating condition variable", 0);
        }
    }

    void SyncManager::DestroyCondVar(BML_CondVar condvar) {
        if (!condvar) {
            ReportInvalidSyncCall("bmlCondVarDestroy", "condition variable handle is NULL");
            return;
        }

        auto *impl = reinterpret_cast<CondVarImpl *>(condvar);

        std::lock_guard<std::mutex> lock(m_CondVarRegistryLock);
        auto it = std::find(m_CondVars.begin(), m_CondVars.end(), impl);
        if (it == m_CondVars.end()) {
            ReportInvalidSyncCall("bmlCondVarDestroy", "condition variable handle is invalid or already destroyed");
            return;
        }

        delete impl;
        m_CondVars.erase(it);
    }

    BML_Result SyncManager::WaitCondVar(BML_CondVar condvar, BML_Mutex mutex) {
        if (!ValidateCondVarHandle(condvar, "bmlCondVarWait") ||
            !ValidateMutexHandle(mutex, "bmlCondVarWait")) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        auto *cv_impl = reinterpret_cast<CondVarImpl *>(condvar);
        auto *mutex_impl = reinterpret_cast<MutexImpl *>(mutex);
        DWORD thread_id = ::GetCurrentThreadId();

        m_DeadlockDetector->OnLockReleased(mutex_impl, thread_id);
        if (m_DeadlockDetector->OnLockWait(mutex_impl, thread_id)) {
            return ReportDeadlock("bmlCondVarWait");
        }

        BOOL result = SleepConditionVariableCS(&cv_impl->cv, &mutex_impl->cs, INFINITE);

        m_DeadlockDetector->OnLockAcquired(mutex_impl, thread_id);
        if (!result) {
            DWORD error = ::GetLastError();
            return SetLastErrorAndReturn(BML_RESULT_UNKNOWN_ERROR, "sync", "bmlCondVarWait",
                                         "SleepConditionVariableCS failed", error);
        }

        return BML_RESULT_OK;
    }

    BML_Result SyncManager::WaitCondVarTimeout(BML_CondVar condvar, BML_Mutex mutex, uint32_t timeout_ms) {
        if (!ValidateCondVarHandle(condvar, "bmlCondVarWaitTimeout") ||
            !ValidateMutexHandle(mutex, "bmlCondVarWaitTimeout")) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        auto *cv_impl = reinterpret_cast<CondVarImpl *>(condvar);
        auto *mutex_impl = reinterpret_cast<MutexImpl *>(mutex);
        DWORD thread_id = ::GetCurrentThreadId();

        m_DeadlockDetector->OnLockReleased(mutex_impl, thread_id);
        if (m_DeadlockDetector->OnLockWait(mutex_impl, thread_id)) {
            return ReportDeadlock("bmlCondVarWaitTimeout");
        }

        BOOL result = SleepConditionVariableCS(&cv_impl->cv, &mutex_impl->cs, timeout_ms);

        m_DeadlockDetector->OnLockAcquired(mutex_impl, thread_id);
        if (!result) {
            DWORD error = ::GetLastError();
            if (error == ERROR_TIMEOUT) {
                return SetLastErrorAndReturn(BML_RESULT_TIMEOUT, "sync", "bmlCondVarWaitTimeout",
                                             "Wait timed out", error);
            }
            return SetLastErrorAndReturn(BML_RESULT_UNKNOWN_ERROR, "sync", "bmlCondVarWaitTimeout",
                                         "SleepConditionVariableCS failed", error);
        }

        return BML_RESULT_OK;
    }

    BML_Result SyncManager::SignalCondVar(BML_CondVar condvar) {
        if (!ValidateCondVarHandle(condvar, "bmlCondVarSignal")) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        auto *impl = reinterpret_cast<CondVarImpl *>(condvar);
        WakeConditionVariable(&impl->cv);
        return BML_RESULT_OK;
    }

    BML_Result SyncManager::BroadcastCondVar(BML_CondVar condvar) {
        if (!ValidateCondVarHandle(condvar, "bmlCondVarBroadcast")) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        auto *impl = reinterpret_cast<CondVarImpl *>(condvar);
        WakeAllConditionVariable(&impl->cv);
        return BML_RESULT_OK;
    }

    bool SyncManager::IsValidCondVar(BML_CondVar condvar) const {
        if (!condvar) {
            return false;
        }

        auto *impl = reinterpret_cast<CondVarImpl *>(condvar);
        std::lock_guard<std::mutex> lock(const_cast<std::mutex &>(m_CondVarRegistryLock));
        return std::find(m_CondVars.begin(), m_CondVars.end(), impl) != m_CondVars.end();
    }

    // ============================================================================
    // SpinLock Operations
    // ============================================================================

    BML_Result SyncManager::CreateSpinLock(BML_SpinLock *out_lock) {
        if (!out_lock) {
            return SetLastErrorAndReturn(BML_RESULT_INVALID_ARGUMENT, "sync", "bmlSpinLockCreate",
                                         "out_lock is NULL", 0);
        }

        try {
            auto *impl = new SpinLockImpl();

            std::lock_guard<std::mutex> lock(m_SpinlockRegistryLock);
            m_Spinlocks.push_back(impl);

            *out_lock = reinterpret_cast<BML_SpinLock>(impl);
            return BML_RESULT_OK;
        } catch (const std::bad_alloc &) {
            return SetLastErrorAndReturn(BML_RESULT_OUT_OF_MEMORY, "sync", "bmlSpinLockCreate",
                                         "Failed to allocate spin lock", 0);
        } catch (...) {
            return SetLastErrorAndReturn(BML_RESULT_UNKNOWN_ERROR, "sync", "bmlSpinLockCreate",
                                         "Unknown error creating spin lock", 0);
        }
    }

    void SyncManager::DestroySpinLock(BML_SpinLock lock) {
        if (!lock) {
            ReportInvalidSyncCall("bmlSpinLockDestroy", "spin lock handle is NULL");
            return;
        }

        auto *impl = reinterpret_cast<SpinLockImpl *>(lock);

        std::lock_guard<std::mutex> guard(m_SpinlockRegistryLock);
        auto it = std::find(m_Spinlocks.begin(), m_Spinlocks.end(), impl);
        if (it == m_Spinlocks.end()) {
            ReportInvalidSyncCall("bmlSpinLockDestroy", "spin lock handle is invalid or already destroyed");
            return;
        }

        delete impl;
        m_Spinlocks.erase(it);
    }

    void SyncManager::LockSpinLock(BML_SpinLock lock) {
        if (!ValidateSpinLockHandle(lock, "bmlSpinLockLock")) {
            return;
        }

        auto *impl = reinterpret_cast<SpinLockImpl *>(lock);
        DWORD thread_id = ::GetCurrentThreadId();

        auto try_fast_acquire = [&]() -> bool {
            uint32_t expected = impl->now_serving.load(std::memory_order_acquire);
            if (impl->next_ticket.compare_exchange_strong(expected, expected + 1,
                                                          std::memory_order_acquire,
                                                          std::memory_order_relaxed)) {
                m_DeadlockDetector->OnLockAcquired(impl, thread_id);
                return true;
            }
            return false;
        };

        if (try_fast_acquire()) {
            return;
        }

        if (m_DeadlockDetector->OnLockWait(impl, thread_id)) {
            ReportDeadlock("bmlSpinLockLock");
            return;
        }

        uint32_t ticket = impl->next_ticket.fetch_add(1, std::memory_order_acq_rel);

        constexpr uint32_t kActiveSpinLimit = 1 << 12;
        uint32_t spin_count = 0;
        for (;;) {
            uint32_t current = impl->now_serving.load(std::memory_order_acquire);
            if (current == ticket) {
                m_DeadlockDetector->OnLockAcquired(impl, thread_id);
                break;
            }

            if (spin_count < kActiveSpinLimit) {
                YieldProcessor();
            } else if (spin_count < kActiveSpinLimit * 2) {
                SwitchToThread();
            } else {
                Sleep(0);
            }
            ++spin_count;
        }
    }

    BML_Bool SyncManager::TryLockSpinLock(BML_SpinLock lock) {
        if (!ValidateSpinLockHandle(lock, "bmlSpinLockTryLock")) {
            return BML_FALSE;
        }

        auto *impl = reinterpret_cast<SpinLockImpl *>(lock);
        DWORD thread_id = ::GetCurrentThreadId();
        uint32_t expected = impl->now_serving.load(std::memory_order_acquire);
        if (impl->next_ticket.compare_exchange_strong(expected, expected + 1,
                                                      std::memory_order_acquire,
                                                      std::memory_order_relaxed)) {
            m_DeadlockDetector->OnLockAcquired(impl, thread_id);
            return BML_TRUE;
        }
        return BML_FALSE;
    }

    void SyncManager::UnlockSpinLock(BML_SpinLock lock) {
        if (!ValidateSpinLockHandle(lock, "bmlSpinLockUnlock")) {
            return;
        }

        auto *impl = reinterpret_cast<SpinLockImpl *>(lock);
        DWORD thread_id = ::GetCurrentThreadId();
        m_DeadlockDetector->OnLockReleased(impl, thread_id);
        impl->now_serving.fetch_add(1, std::memory_order_release);
    }

    bool SyncManager::IsValidSpinLock(BML_SpinLock lock) const {
        if (!lock) {
            return false;
        }

        auto *impl = reinterpret_cast<SpinLockImpl *>(lock);
        std::lock_guard<std::mutex> guard(const_cast<std::mutex &>(m_SpinlockRegistryLock));
        return std::find(m_Spinlocks.begin(), m_Spinlocks.end(), impl) != m_Spinlocks.end();
    }

    bool SyncManager::ValidateMutexHandle(BML_Mutex mutex, const char *api) const {
        if (!mutex) {
            ReportInvalidSyncCall(api, "mutex handle is NULL");
            return false;
        }
        if (!IsValidMutex(mutex)) {
            ReportInvalidSyncCall(api, "mutex handle is invalid or stale");
            return false;
        }
        return true;
    }

    bool SyncManager::ValidateRwLockHandle(BML_RwLock lock, const char *api) const {
        if (!lock) {
            ReportInvalidSyncCall(api, "rwlock handle is NULL");
            return false;
        }
        if (!IsValidRwLock(lock)) {
            ReportInvalidSyncCall(api, "rwlock handle is invalid or stale");
            return false;
        }
        return true;
    }

    bool SyncManager::ValidateSemaphoreHandle(BML_Semaphore semaphore, const char *api) const {
        if (!semaphore) {
            ReportInvalidSyncCall(api, "semaphore handle is NULL");
            return false;
        }
        if (!IsValidSemaphore(semaphore)) {
            ReportInvalidSyncCall(api, "semaphore handle is invalid or stale");
            return false;
        }
        return true;
    }

    bool SyncManager::ValidateTlsHandle(BML_TlsKey key, const char *api) const {
        if (!key) {
            ReportInvalidSyncCall(api, "TLS key is NULL");
            return false;
        }
        if (!IsValidTlsKey(key)) {
            ReportInvalidSyncCall(api, "TLS key is invalid or stale");
            return false;
        }
        return true;
    }

    bool SyncManager::ValidateCondVarHandle(BML_CondVar condvar, const char *api) const {
        if (!condvar) {
            ReportInvalidSyncCall(api, "condition variable handle is NULL");
            return false;
        }
        if (!IsValidCondVar(condvar)) {
            ReportInvalidSyncCall(api, "condition variable handle is invalid or stale");
            return false;
        }
        return true;
    }

    bool SyncManager::ValidateSpinLockHandle(BML_SpinLock lock, const char *api) const {
        if (!lock) {
            ReportInvalidSyncCall(api, "spin lock handle is NULL");
            return false;
        }
        if (!IsValidSpinLock(lock)) {
            ReportInvalidSyncCall(api, "spin lock handle is invalid or stale");
            return false;
        }
        return true;
    }

    void SyncManager::ReportLockMisuse(const char *api, const char *message) const {
        ReportInvalidSyncCall(api, message);
    }

    BML_Result SyncManager::ReportDeadlock(const char *api) const {
        return SetLastErrorAndReturn(BML_RESULT_SYNC_DEADLOCK, "sync", api,
                                     "Potential deadlock detected", 0);
    }

    // ============================================================================
    // Capabilities
    // ============================================================================

    BML_Result SyncManager::GetCaps(BML_SyncCaps *out_caps) {
        if (!out_caps) {
            return SetLastErrorAndReturn(BML_RESULT_INVALID_ARGUMENT, "sync", "bmlSyncGetCaps",
                                         "out_caps is NULL", 0);
        }

        out_caps->struct_size = sizeof(BML_SyncCaps);
        out_caps->api_version = bmlGetApiVersion();
        out_caps->capability_flags =
            BML_SYNC_CAP_MUTEX |
            BML_SYNC_CAP_RWLOCK |
            BML_SYNC_CAP_ATOMICS |
            BML_SYNC_CAP_SEMAPHORE |
            BML_SYNC_CAP_TLS |
            BML_SYNC_CAP_CONDVAR |
            BML_SYNC_CAP_SPINLOCK;

        return BML_RESULT_OK;
    }
} // namespace BML::Core

