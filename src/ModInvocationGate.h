#ifndef BML_MODINVOCATIONGATE_H
#define BML_MODINVOCATIONGATE_H

#include <cstddef>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>

namespace BML {

class ModInvocationGate {
public:
    class CallLock {
    public:
        CallLock() = default;

        explicit CallLock(ModInvocationGate &gate)
            : m_Gate(&gate) {
            for (auto &entry : s_CallEntries) {
                if (entry.Gate == &gate) {
                    ++entry.Depth;
                    return;
                }
            }

            m_Lock = std::shared_lock<std::shared_mutex>(gate.m_Mutex);
            s_CallEntries.push_back({&gate, 1});
            m_OwnsUnderlyingLock = true;
        }

        CallLock(const CallLock &) = delete;
        CallLock &operator=(const CallLock &) = delete;

        CallLock(CallLock &&other) noexcept {
            MoveFrom(other);
        }

        CallLock &operator=(CallLock &&other) noexcept {
            if (this != &other) {
                unlock();
                MoveFrom(other);
            }
            return *this;
        }

        ~CallLock() {
            unlock();
        }

        void unlock() {
            if (!m_Gate)
                return;

            for (auto it = s_CallEntries.begin(); it != s_CallEntries.end(); ++it) {
                if (it->Gate != m_Gate)
                    continue;
                if (--it->Depth == 0)
                    s_CallEntries.erase(it);
                break;
            }
            if (m_OwnsUnderlyingLock)
                m_Lock.unlock();
            m_Gate = nullptr;
            m_OwnsUnderlyingLock = false;
        }

    private:
        void MoveFrom(CallLock &other) {
            m_Gate = other.m_Gate;
            m_Lock = std::move(other.m_Lock);
            m_OwnsUnderlyingLock = other.m_OwnsUnderlyingLock;
            other.m_Gate = nullptr;
            other.m_OwnsUnderlyingLock = false;
        }

        ModInvocationGate *m_Gate = nullptr;
        std::shared_lock<std::shared_mutex> m_Lock;
        bool m_OwnsUnderlyingLock = false;
    };

    using MutationLock = std::unique_lock<std::shared_mutex>;

    CallLock LockCall() {
        return CallLock(*this);
    }

    MutationLock LockMutation() {
        return MutationLock(m_Mutex);
    }

    bool IsCallActiveOnCurrentThread() const {
        for (const auto &entry : s_CallEntries) {
            if (entry.Gate == this)
                return true;
        }
        return false;
    }

private:
    struct CallEntry {
        ModInvocationGate *Gate = nullptr;
        size_t Depth = 0;
    };
    inline static thread_local std::vector<CallEntry> s_CallEntries;
    std::shared_mutex m_Mutex;
};

} // namespace BML

#endif
