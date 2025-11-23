/**
 * @file RefCount.h
 * @brief Intrusive reference counter with resurrection guard and clear memory model notes.
 */
#ifndef BML_REFCOUNT_H
#define BML_REFCOUNT_H

#include <atomic>
#include <cstdint>

namespace BML {
    class RefCount {
    public:
        // Start at 1 by default for intrusive objects that are born owned; use 0 if you really pool-manage.
        explicit RefCount(uint32_t initial = 0) noexcept : m_RefCount(initial) {}
        ~RefCount() = default;

        RefCount(const RefCount &) = delete;
        RefCount &operator=(const RefCount &) = delete;

        // Reset for object pools; best used when count is 0.
        uint32_t Reset(uint32_t v = 0) noexcept {
            m_RefCount.store(v, std::memory_order_relaxed);
            return v;
        }

        // Fast path bump: relaxed is sufficient (doesn't publish object state).
        uint32_t AddRef() noexcept {
            return m_RefCount.fetch_add(1, std::memory_order_relaxed) + 1;
        }

        // Try to add only if object is still "live" (count > 0). Prevents resurrection.
        // Returns true and bumps the count on success.
        bool TryAddRef() noexcept {
            uint32_t cur = m_RefCount.load(std::memory_order_relaxed);
            while (cur != 0) {
                if (m_RefCount.compare_exchange_weak(
                    cur, cur + 1,
                    std::memory_order_relaxed,  // success
                    std::memory_order_relaxed)) // failure
                    return true;
                // cur is reloaded; loop
            }
            return false;
        }

        // Returns the new (post-decrement) count.
        // Use pattern:
        // if (rc.Release() == 0) { std::atomic_thread_fence(std::memory_order_acquire); delete this; }
        uint32_t Release() noexcept {
            return m_RefCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
        }

        uint32_t GetCount() const noexcept {
            // relaxed is fine for approximate diagnostics; not a liveness check
            return m_RefCount.load(std::memory_order_relaxed);
        }

    private:
        std::atomic<uint32_t> m_RefCount;
    };
} // namespace BML

#endif // BML_REFCOUNT_H
