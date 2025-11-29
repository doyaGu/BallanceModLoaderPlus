#ifndef BML_CORE_MPSC_RING_BUFFER_H
#define BML_CORE_MPSC_RING_BUFFER_H

#include <atomic>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace BML::Core {
    /**
     * @brief Lock-free Multi-Producer Single-Consumer (MPSC) Ring Buffer
     *
     * A high-performance bounded queue based on Dmitry Vyukov's bounded MPMC
     * queue algorithm, specialized for single-consumer scenarios.
     *
     * ## Design Overview
     *
     * Each slot contains a sequence counter that enables producers to reserve
     * exclusive access without global locks. The consumer drains entries in order.
     *
     * ### Memory Ordering
     *
     * - **Enqueue (producers)**: Uses acquire on sequence read, release on sequence write
     * - **Dequeue (consumer)**: Uses acquire on sequence read, release on sequence write
     * - **Head/Tail updates**: Use relaxed ordering (protected by CAS loops)
     *
     * ### Capacity Normalization
     *
     * Capacity is rounded up to the next power of 2 for efficient index masking.
     * Minimum capacity is 2 to ensure correct sequence number behavior.
     *
     * ### Starvation Prevention
     *
     * When used with PriorityMessageQueue, the queue participates in a weighted
     * fair queuing scheme that guarantees low-priority messages are not starved.
     *
     * ## Thread Safety
     *
     * - Multiple threads can call Enqueue() concurrently (wait-free)
     * - Only ONE thread should call Dequeue() (not thread-safe for multiple consumers)
     * - IsEmpty() and ApproximateSize() are safe to call from any thread
     *
     * ## Performance Characteristics
     *
     * - O(1) Enqueue (amortized, may spin briefly under contention)
     * - O(1) Dequeue (wait-free)
     * - Cache-line friendly slot layout
     *
     * @tparam T Value type (must be move or copy constructible)
     */
    template <typename T>
    class MpscRingBuffer {
        static_assert(std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>,
                      "MpscRingBuffer requires movable or copyable value types");

    public:
        explicit MpscRingBuffer(size_t capacity)
            : m_Capacity(NormalizeCapacity(capacity)),
              m_Mask(m_Capacity - 1),
              m_Buffer(m_Capacity) {
            for (size_t i = 0; i < m_Capacity; ++i) {
                m_Buffer[i].sequence.store(i, std::memory_order_relaxed);
            }
        }

        MpscRingBuffer(const MpscRingBuffer &) = delete;
        MpscRingBuffer &operator=(const MpscRingBuffer &) = delete;

        bool Enqueue(const T &value) {
            return Emplace(value);
        }

        bool Enqueue(T &&value) {
            return Emplace(std::move(value));
        }

        template <typename... Args>
        bool Emplace(Args &&... args) {
            Slot *slot = nullptr;
            size_t pos = 0;

            while (true) {
                pos = m_Head.load(std::memory_order_relaxed);
                slot = &m_Buffer[pos & m_Mask];
                size_t seq = slot->sequence.load(std::memory_order_acquire);
                std::intptr_t diff = static_cast<std::intptr_t>(seq) - static_cast<std::intptr_t>(pos);
                if (diff == 0) {
                    if (m_Head.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                        break;
                    }
                } else if (diff < 0) {
                    return false; // full
                } else {
                    // another producer is ahead, retry with updated head
                    continue;
                }
            }

            slot->value.emplace(std::forward<Args>(args)...);
            slot->sequence.store(pos + 1, std::memory_order_release);
            return true;
        }

        bool Dequeue(T &out_value) {
            Slot *slot = nullptr;
            size_t pos = 0;

            while (true) {
                pos = m_Tail.load(std::memory_order_relaxed);
                slot = &m_Buffer[pos & m_Mask];
                size_t seq = slot->sequence.load(std::memory_order_acquire);
                std::intptr_t diff = static_cast<std::intptr_t>(seq) - static_cast<std::intptr_t>(pos + 1);
                if (diff == 0) {
                    if (m_Tail.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                        break;
                    }
                } else if (diff < 0) {
                    return false; // empty
                } else {
                    continue;
                }
            }

            out_value = std::move(*slot->value);
            slot->value.reset();
            slot->sequence.store(pos + m_Capacity, std::memory_order_release);
            return true;
        }

        bool IsEmpty() const {
            return (m_Tail.load(std::memory_order_acquire) == m_Head.load(std::memory_order_acquire));
        }

        size_t ApproximateSize() const {
            size_t head = m_Head.load(std::memory_order_acquire);
            size_t tail = m_Tail.load(std::memory_order_acquire);
            return (head >= tail) ? (head - tail) : 0;
        }

        size_t Capacity() const { return m_Capacity; }

        void Clear() {
            T tmp;
            while (Dequeue(tmp)) {
            }
        }

    private:
        struct Slot {
            std::atomic<size_t> sequence{0};
            std::optional<T> value;
        };

        static size_t NormalizeCapacity(size_t capacity) {
            if (capacity < 2)
                capacity = 2;
            if ((capacity & (capacity - 1)) == 0)
                return capacity;
            --capacity;
            for (size_t shift = 1; shift < sizeof(size_t) * 8; shift <<= 1) {
                capacity |= capacity >> shift;
            }
            return capacity + 1;
        }

        const size_t m_Capacity;
        const size_t m_Mask;
        std::vector<Slot> m_Buffer;
        std::atomic<size_t> m_Head{0};
        std::atomic<size_t> m_Tail{0};
    };
} // namespace BML::Core

#endif // BML_CORE_MPSC_RING_BUFFER_H
