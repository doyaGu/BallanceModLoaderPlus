#ifndef BML_CORE_SPSC_RING_BUFFER_H
#define BML_CORE_SPSC_RING_BUFFER_H

#include <atomic>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace BML::Core {
    // Lock-free single-producer/single-consumer ring buffer.
    // Capacity is rounded up to the next power of two and one slot is reserved
    // to differentiate between full and empty states.
    template <typename T>
    class SpscRingBuffer {
        static_assert(std::is_copy_constructible_v<T> || std::is_move_constructible_v<T>,
                      "SpscRingBuffer requires movable or copyable value types");

    public:
        explicit SpscRingBuffer(size_t capacity)
            : m_Capacity(NormalizeCapacity(capacity)),
              m_Mask(m_Capacity - 1),
              m_Buffer(m_Capacity) {}

        SpscRingBuffer(const SpscRingBuffer &) = delete;
        SpscRingBuffer &operator=(const SpscRingBuffer &) = delete;

        bool Enqueue(const T &value) {
            return Emplace(value);
        }

        bool Enqueue(T &&value) {
            return Emplace(std::move(value));
        }

        template <typename... Args>
        bool Emplace(Args &&... args) {
            const size_t head = m_Head.load(std::memory_order_relaxed);
            const size_t next = (head + 1) & m_Mask;
            if (next == m_Tail.load(std::memory_order_acquire)) {
                return false; // full
            }

            m_Buffer[head].emplace(std::forward<Args>(args)...);
            m_Head.store(next, std::memory_order_release);
            return true;
        }

        bool Dequeue(T &out_value) {
            const size_t tail = m_Tail.load(std::memory_order_relaxed);
            if (tail == m_Head.load(std::memory_order_acquire)) {
                return false; // empty
            }

            auto &slot = m_Buffer[tail];
            out_value = std::move(*slot);
            slot.reset();
            m_Tail.store((tail + 1) & m_Mask, std::memory_order_release);
            return true;
        }

        bool Peek(T &out_value) const {
            const size_t tail = m_Tail.load(std::memory_order_relaxed);
            if (tail == m_Head.load(std::memory_order_acquire)) {
                return false;
            }
            out_value = *m_Buffer[tail];
            return true;
        }

        bool IsEmpty() const {
            return m_Head.load(std::memory_order_acquire) == m_Tail.load(std::memory_order_acquire);
        }

        bool IsFull() const {
            const size_t head = m_Head.load(std::memory_order_acquire);
            const size_t tail = m_Tail.load(std::memory_order_acquire);
            return ((head + 1) & m_Mask) == tail;
        }

        size_t Capacity() const { return m_Capacity - 1; }

        void Clear() {
            size_t tail = m_Tail.load(std::memory_order_relaxed);
            const size_t head = m_Head.load(std::memory_order_acquire);
            while (tail != head) {
                m_Buffer[tail].reset();
                tail = (tail + 1) & m_Mask;
            }
            m_Tail.store(head, std::memory_order_release);
        }

    private:
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
        std::vector<std::optional<T>> m_Buffer;
        std::atomic<size_t> m_Head{0};
        std::atomic<size_t> m_Tail{0};
    };
} // namespace BML::Core

#endif // BML_CORE_SPSC_RING_BUFFER_H
