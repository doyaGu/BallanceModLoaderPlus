#ifndef BML_CORE_DEADLOCK_DETECTOR_H
#define BML_CORE_DEADLOCK_DETECTOR_H

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

namespace BML::Core {

    class DeadlockDetector {
    public:
        bool OnLockWait(void *lock_key, DWORD thread_id) {
            std::lock_guard<std::mutex> guard(m_mutex);
            ThreadState &thread_state = m_threads[thread_id];
            if (m_deadlocked_threads.erase(thread_id) != 0) {
                thread_state.waiting_lock = nullptr;
                return true;
            }
            thread_state.waiting_lock = lock_key;
            std::unordered_set<void *> lock_visited;
            std::unordered_set<DWORD> thread_visited;
            bool has_cycle = DetectCycle(lock_key, thread_id, lock_visited, thread_visited);
            if (has_cycle) {
                thread_state.waiting_lock = nullptr;
                m_deadlocked_threads.insert(thread_id);
                for (DWORD cycle_thread : thread_visited) {
                    m_deadlocked_threads.insert(cycle_thread);
                }
            }
            return has_cycle;
        }

        void OnLockAcquired(void *lock_key, DWORD thread_id) {
            std::lock_guard<std::mutex> guard(m_mutex);
            m_deadlocked_threads.erase(thread_id);
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
                    m_deadlocked_threads.erase(thread_id);
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
                    m_deadlocked_threads.erase(thread_id);
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

        bool ConsumeDeadlock(DWORD thread_id) {
            std::lock_guard<std::mutex> guard(m_mutex);
            return m_deadlocked_threads.erase(thread_id) != 0;
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
        std::unordered_set<DWORD> m_deadlocked_threads;
    };

} // namespace BML::Core

#endif // BML_CORE_DEADLOCK_DETECTOR_H
