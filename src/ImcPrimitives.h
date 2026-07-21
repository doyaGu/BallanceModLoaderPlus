#ifndef BML_IMCPRIMITIVES_H
#define BML_IMCPRIMITIVES_H

#include "BML/Imc.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

namespace BML::ImcDetail {

inline uint32_t HashMix(uint32_t value) noexcept {
    value ^= value >> 16;
    value *= 0x85ebca6bu;
    value ^= value >> 13;
    value *= 0xc2b2ae35u;
    value ^= value >> 16;
    return value;
}

inline uint32_t ComputeId(const char *text) noexcept {
    constexpr uint32_t prime1 = 0x9e3779b1u;
    constexpr uint32_t prime3 = 0xc2b2ae3du;
    uint32_t hash = 0x165667b1u;
    const char *cursor = text;
    while (*cursor) {
        hash += static_cast<uint32_t>(static_cast<unsigned char>(*cursor)) * prime3;
        hash = ((hash << 17) | (hash >> 15)) * prime1;
        ++cursor;
    }
    hash ^= static_cast<uint32_t>(cursor - text);
    hash = HashMix(hash);
    return hash == 0 ? 1u : hash;
}

class BufferStorage {
public:
    BufferStorage() = default;
    BufferStorage(const BufferStorage &) = delete;
    BufferStorage &operator=(const BufferStorage &) = delete;
    BufferStorage(BufferStorage &&) noexcept = default;
    BufferStorage &operator=(BufferStorage &&) noexcept = default;

    bool Resize(size_t size) {
        if (size <= m_Inline.size()) {
            m_Heap.reset();
            m_Size = size;
            return true;
        }
        try {
            m_Heap = std::make_unique<uint8_t[]>(size);
            m_Size = size;
            return true;
        } catch (...) {
            Reset();
            return false;
        }
    }

    bool CopyFrom(const void *data, size_t size) {
        if (size != 0 && !data)
            return false;
        if (!Resize(size))
            return false;
        if (size != 0)
            std::memcpy(Data(), data, size);
        return true;
    }

    void Reset() noexcept {
        m_Heap.reset();
        m_Size = 0;
    }

    void *Data() noexcept { return m_Heap ? m_Heap.get() : m_Inline.data(); }
    const void *Data() const noexcept { return m_Heap ? m_Heap.get() : m_Inline.data(); }
    size_t Size() const noexcept { return m_Size; }

private:
    std::array<uint8_t, BML_IMC_INLINE_PAYLOAD_SIZE> m_Inline{};
    std::unique_ptr<uint8_t[]> m_Heap;
    size_t m_Size = 0;
};

/* Vyukov bounded queue.  Both head and tail use CAS, so this implementation
 * safely supports multiple producers and multiple consumers even though IMC
 * normally drains it from one game thread. */
template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(size_t capacity)
        : m_Capacity(NormalizeCapacity(capacity)),
          m_Mask(m_Capacity - 1),
          m_Slots(std::make_unique<Slot[]>(m_Capacity)) {
        for (size_t i = 0; i < m_Capacity; ++i)
            m_Slots[i].Sequence.store(i, std::memory_order_relaxed);
    }

    BoundedQueue(const BoundedQueue &) = delete;
    BoundedQueue &operator=(const BoundedQueue &) = delete;

    template <typename U>
    bool Enqueue(U &&value) {
        size_t position = m_Head.load(std::memory_order_relaxed);
        for (;;) {
            Slot &slot = m_Slots[position & m_Mask];
            const size_t sequence = slot.Sequence.load(std::memory_order_acquire);
            const intptr_t difference = static_cast<intptr_t>(sequence) -
                                        static_cast<intptr_t>(position);
            if (difference == 0) {
                if (m_Head.compare_exchange_weak(position, position + 1,
                                                 std::memory_order_relaxed,
                                                 std::memory_order_relaxed)) {
                    slot.Value.emplace(std::forward<U>(value));
                    slot.Sequence.store(position + 1, std::memory_order_release);
                    return true;
                }
            } else if (difference < 0) {
                return false;
            } else {
                position = m_Head.load(std::memory_order_relaxed);
            }
        }
    }

    bool Dequeue(T &value) {
        size_t position = m_Tail.load(std::memory_order_relaxed);
        for (;;) {
            Slot &slot = m_Slots[position & m_Mask];
            const size_t sequence = slot.Sequence.load(std::memory_order_acquire);
            const intptr_t difference = static_cast<intptr_t>(sequence) -
                                        static_cast<intptr_t>(position + 1);
            if (difference == 0) {
                if (m_Tail.compare_exchange_weak(position, position + 1,
                                                 std::memory_order_relaxed,
                                                 std::memory_order_relaxed)) {
                    value = std::move(*slot.Value);
                    slot.Value.reset();
                    slot.Sequence.store(position + m_Capacity,
                                        std::memory_order_release);
                    return true;
                }
            } else if (difference < 0) {
                return false;
            } else {
                position = m_Tail.load(std::memory_order_relaxed);
            }
        }
    }

    size_t ApproximateSize() const noexcept {
        const size_t head = m_Head.load(std::memory_order_acquire);
        const size_t tail = m_Tail.load(std::memory_order_acquire);
        return head >= tail ? head - tail : 0;
    }

    size_t Capacity() const noexcept { return m_Capacity; }

private:
    struct Slot {
        std::atomic<size_t> Sequence{0};
        std::optional<T> Value;
    };

    static size_t NormalizeCapacity(size_t capacity) noexcept {
        capacity = (std::max)(capacity, size_t{2});
        if ((capacity & (capacity - 1)) == 0)
            return capacity;
        --capacity;
        for (size_t shift = 1; shift < sizeof(size_t) * 8; shift <<= 1)
            capacity |= capacity >> shift;
        return capacity + 1;
    }

    const size_t m_Capacity;
    const size_t m_Mask;
    std::unique_ptr<Slot[]> m_Slots;
    alignas(64) std::atomic<size_t> m_Head{0};
    alignas(64) std::atomic<size_t> m_Tail{0};
};

template <typename T, size_t Capacity>
class ObjectPool {
public:
    ObjectPool() : m_Free(Capacity) {
        for (uint32_t i = 0; i < Capacity; ++i)
            m_Free.Enqueue(i);
    }

    ObjectPool(const ObjectPool &) = delete;
    ObjectPool &operator=(const ObjectPool &) = delete;

    template <typename... Args>
    T *Construct(Args &&...args) {
        uint32_t index = 0;
        if (!m_Free.Dequeue(index))
            return nullptr;
        T *object = new(&m_Storage[index]) T(std::forward<Args>(args)...);
        m_Live[index].store(true, std::memory_order_release);
        return object;
    }

    void Destroy(T *object) noexcept {
        if (!object)
            return;
        const auto *base = reinterpret_cast<const std::byte *>(m_Storage.data());
        const auto *address = reinterpret_cast<const std::byte *>(object);
        const ptrdiff_t distance = address - base;
        if (distance < 0 || distance % sizeof(Storage) != 0)
            return;
        const size_t index = static_cast<size_t>(distance) / sizeof(Storage);
        if (index >= Capacity || !m_Live[index].exchange(false, std::memory_order_acq_rel))
            return;
        object->~T();
        m_Free.Enqueue(static_cast<uint32_t>(index));
    }

    bool Owns(const T *object) const noexcept {
        const auto *base = reinterpret_cast<const std::byte *>(m_Storage.data());
        const auto *end = base + sizeof(Storage) * Capacity;
        const auto *address = reinterpret_cast<const std::byte *>(object);
        return address >= base && address < end &&
               ((address - base) % sizeof(Storage) == 0);
    }

private:
    using Storage = std::aligned_storage_t<sizeof(T), alignof(T)>;
    std::array<Storage, Capacity> m_Storage{};
    std::array<std::atomic<bool>, Capacity> m_Live{};
    BoundedQueue<uint32_t> m_Free;
};

} // namespace BML::ImcDetail

#endif // BML_IMCPRIMITIVES_H
