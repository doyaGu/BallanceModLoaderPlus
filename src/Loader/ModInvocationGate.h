#ifndef BML_MODINVOCATIONGATE_H
#define BML_MODINVOCATIONGATE_H

#include <cstddef>
#include <mutex>
#include <memory>
#include <shared_mutex>
#include <utility>
#include <vector>

namespace BML {

class ModInvocationGate {
public:
    class CallLock {
    public:
        CallLock() = default;

        explicit CallLock(ModInvocationGate &gate) : m_Gate(&gate) {
            for (auto &entry : s_CallEntries) {
                if (entry.Gate == &gate && entry.Depth) { ++entry.Depth; return; }
            }
            bool mutation = false;
            for (const auto &entry : s_MutationEntries) if (entry.Gate == &gate) mutation = true;
            if (!mutation) {
                m_Lock = std::shared_lock<std::shared_mutex>(gate.m_Mutex);
                m_OwnsUnderlyingLock = true;
            }
            for (auto &entry : s_CallEntries) {
                if (entry.Gate == &gate && !entry.Depth) {
                    entry.Depth = 1; m_IsOutermost = true; return;
                }
            }
            for (auto &entry : s_CallEntries) {
                if (!entry.Depth) { entry.Gate = &gate; entry.Depth = 1; m_IsOutermost = true; return; }
            }
            s_CallEntries.push_back({&gate, 1, {}});
            m_IsOutermost = true;
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

        bool IsOutermost() const noexcept {
            return m_Gate && m_IsOutermost;
        }

        void unlock() {
            if (!m_Gate)
                return;

            for (auto it = s_CallEntries.begin(); it != s_CallEntries.end(); ++it) {
                if (it->Gate != m_Gate)
                    continue;
                if (--it->Depth == 0)
                    it->Borrows.clear();
                break;
            }
            if (m_OwnsUnderlyingLock)
                m_Lock.unlock();
            m_Gate = nullptr;
            m_OwnsUnderlyingLock = false;
            m_IsOutermost = false;
        }

    private:
        void MoveFrom(CallLock &other) {
            m_Gate = other.m_Gate;
            m_Lock = std::move(other.m_Lock);
            m_OwnsUnderlyingLock = other.m_OwnsUnderlyingLock;
            m_IsOutermost = other.m_IsOutermost;
            other.m_Gate = nullptr;
            other.m_OwnsUnderlyingLock = false;
            other.m_IsOutermost = false;
        }

        ModInvocationGate *m_Gate = nullptr;
        std::shared_lock<std::shared_mutex> m_Lock;
        bool m_OwnsUnderlyingLock = false;
        bool m_IsOutermost = false;
    };

    class MutationLock {
    public:
        MutationLock() = default;

        explicit MutationLock(ModInvocationGate &gate)
            : m_Gate(&gate) {
            for (auto &entry : s_MutationEntries) {
                if (entry.Gate == &gate) {
                    ++entry.Depth;
                    return;
                }
            }

            m_Lock = std::unique_lock<std::shared_mutex>(gate.m_Mutex);
            s_MutationEntries.push_back({&gate, 1});
            m_OwnsUnderlyingLock = true;
        }

        MutationLock(const MutationLock &) = delete;
        MutationLock &operator=(const MutationLock &) = delete;

        MutationLock(MutationLock &&other) noexcept {
            MoveFrom(other);
        }

        MutationLock &operator=(MutationLock &&other) noexcept {
            if (this != &other) {
                unlock();
                MoveFrom(other);
            }
            return *this;
        }

        ~MutationLock() {
            unlock();
        }

        void unlock() {
            if (!m_Gate)
                return;

            for (auto it = s_MutationEntries.begin(); it != s_MutationEntries.end(); ++it) {
                if (it->Gate != m_Gate)
                    continue;
                if (--it->Depth == 0)
                    s_MutationEntries.erase(it);
                break;
            }
            if (m_OwnsUnderlyingLock)
                m_Lock.unlock();
            m_Gate = nullptr;
            m_OwnsUnderlyingLock = false;
        }

    private:
        void MoveFrom(MutationLock &other) {
            m_Gate = other.m_Gate;
            m_Lock = std::move(other.m_Lock);
            m_OwnsUnderlyingLock = other.m_OwnsUnderlyingLock;
            other.m_Gate = nullptr;
            other.m_OwnsUnderlyingLock = false;
        }

        ModInvocationGate *m_Gate = nullptr;
        std::unique_lock<std::shared_mutex> m_Lock;
        bool m_OwnsUnderlyingLock = false;
    };

    CallLock LockCall() {
        return CallLock(*this);
    }

    MutationLock LockMutation() {
        return MutationLock(*this);
    }

    bool IsCallActiveOnCurrentThread() const {
        for (const auto &entry : s_CallEntries) {
            if (entry.Gate == this && entry.Depth)
                return true;
        }
        return false;
    }

    // Host-owned immutable snapshots are pinned until the outer callback exits.
    // Inactive entries retain capacity, keeping stable reads allocation-free.
    void HoldBorrow(std::shared_ptr<const void> value) {
        for (auto &entry : s_CallEntries) {
            if (entry.Gate != this || !entry.Depth) continue;
            for (const auto &held : entry.Borrows) if (held.get() == value.get()) return;
            entry.Borrows.push_back(std::move(value));
            return;
        }
    }
    size_t CallDepth() const noexcept {
        for (const auto &entry : s_CallEntries) if (entry.Gate == this) return entry.Depth;
        return 0;
    }

    bool IsMutationActiveOnCurrentThread() const {
        for (const auto &entry : s_MutationEntries) {
            if (entry.Gate == this)
                return true;
        }
        return false;
    }

private:
    struct CallEntry {
        ModInvocationGate *Gate = nullptr;
        size_t Depth = 0;
        std::vector<std::shared_ptr<const void>> Borrows;
    };
    static thread_local std::vector<CallEntry> s_CallEntries;
    static thread_local std::vector<CallEntry> s_MutationEntries;
    std::shared_mutex m_Mutex;
};

} // namespace BML

#endif
