#ifndef BML_DEVTOOLS_CONCURRENT_RING_BUFFER_H
#define BML_DEVTOOLS_CONCURRENT_RING_BUFFER_H

#include <atomic>
#include <cstddef>
#include <new>
#include <type_traits>

namespace devtools {

/// Lock-free bounded MPSC (multi-producer, single-consumer) ring buffer.
/// Uses Vyukov sequence-based algorithm for safe concurrent writes.
/// N must be a power of two.
template <typename T, size_t N>
class ConcurrentRingBuffer {
    static_assert((N & (N - 1)) == 0, "N must be power of 2");
    static_assert(N >= 2, "N must be at least 2");
    static constexpr size_t kMask = N - 1;

    struct Slot {
        std::atomic<size_t> sequence;
        T data;
    };

    alignas(64) Slot m_Slots[N];
    alignas(64) std::atomic<size_t> m_WriteHead{0};
    alignas(64) size_t m_ReadTail{0};

public:
    ConcurrentRingBuffer() {
        for (size_t i = 0; i < N; ++i)
            m_Slots[i].sequence.store(i, std::memory_order_relaxed);
    }

    bool TryPush(const T &value) {
        size_t pos = m_WriteHead.load(std::memory_order_relaxed);
        for (;;) {
            Slot &slot = m_Slots[pos & kMask];
            size_t seq = slot.sequence.load(std::memory_order_acquire);
            auto diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
            if (diff == 0) {
                if (m_WriteHead.compare_exchange_weak(
                        pos, pos + 1, std::memory_order_relaxed)) {
                    slot.data = value;
                    slot.sequence.store(pos + 1, std::memory_order_release);
                    return true;
                }
            } else if (diff < 0) {
                return false;
            } else {
                pos = m_WriteHead.load(std::memory_order_relaxed);
            }
        }
    }

    size_t Drain(T *out, size_t max_count) {
        size_t count = 0;
        while (count < max_count) {
            Slot &slot = m_Slots[m_ReadTail & kMask];
            size_t seq = slot.sequence.load(std::memory_order_acquire);
            auto diff = static_cast<intptr_t>(seq)
                      - static_cast<intptr_t>(m_ReadTail + 1);
            if (diff == 0) {
                out[count++] = std::move(slot.data);
                slot.sequence.store(m_ReadTail + N, std::memory_order_release);
                ++m_ReadTail;
            } else {
                break;
            }
        }
        return count;
    }
};

} // namespace devtools

#endif // BML_DEVTOOLS_CONCURRENT_RING_BUFFER_H
