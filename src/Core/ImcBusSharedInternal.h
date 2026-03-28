#ifndef BML_CORE_IMC_BUS_SHARED_INTERNAL_H
#define BML_CORE_IMC_BUS_SHARED_INTERNAL_H

#include "ImcBus.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Context.h"
#include "MpscRingBuffer.h"

namespace BML::Core {
    inline constexpr char kImcLogCategory[] = "imc.bus";
    inline constexpr size_t kDefaultQueueCapacity = 256;
    inline constexpr size_t kMaxQueueCapacity = 16384;
    inline constexpr size_t kInlinePayloadBytes = 256;
    inline constexpr size_t kDefaultRpcQueueCapacity = 256;
    inline constexpr size_t kPriorityLevels = 4;

    inline uint32_t HashMix(uint32_t h) noexcept {
        h ^= h >> 16;
        h *= 0x85ebca6b;
        h ^= h >> 13;
        h *= 0xc2b2ae35;
        h ^= h >> 16;
        return h;
    }

    inline uint32_t ComputeId(const char *str) noexcept {
        constexpr uint32_t kPrime1 = 0x9E3779B1u;
        constexpr uint32_t kPrime3 = 0xC2B2AE3Du;

        uint32_t hash = 0x165667B1u;
        const char *p = str;

        while (*p) {
            hash += static_cast<uint32_t>(*p) * kPrime3;
            hash = ((hash << 17) | (hash >> 15)) * kPrime1;
            ++p;
        }

        hash ^= static_cast<uint32_t>(p - str);
        hash = HashMix(hash);
        return hash != 0 ? hash : 1u;
    }

    inline uint64_t GetTimestampNsRaw() noexcept {
        using namespace std::chrono;
        return static_cast<uint64_t>(
            duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
    }

    inline uint64_t GetTimestampNs() noexcept {
        return GetTimestampNsRaw();
    }

    template <typename T, size_t N>
    class StackVec {
    public:
        StackVec() = default;
        StackVec(const StackVec &) = delete;
        StackVec &operator=(const StackVec &) = delete;

        void push_back(T value) {
            if (m_Size < N) {
                m_Inline[m_Size++] = value;
                return;
            }

            if (m_Size == N) {
                m_Overflow.reserve(N * 2);
                m_Overflow.assign(m_Inline, m_Inline + N);
            }
            m_Overflow.push_back(value);
            ++m_Size;
        }

        void reserve(size_t) {}

        [[nodiscard]] size_t size() const noexcept { return m_Size; }
        [[nodiscard]] bool empty() const noexcept { return m_Size == 0; }

        T *begin() noexcept { return m_Size <= N ? m_Inline : m_Overflow.data(); }
        T *end() noexcept { return begin() + m_Size; }
        const T *begin() const noexcept { return m_Size <= N ? m_Inline : m_Overflow.data(); }
        const T *end() const noexcept { return begin() + m_Size; }

        T &operator[](size_t i) noexcept { return begin()[i]; }
        const T &operator[](size_t i) const noexcept { return begin()[i]; }

    private:
        T m_Inline[N]{};
        std::vector<T> m_Overflow;
        size_t m_Size = 0;
    };

    struct BufferStorage {
        enum class Kind { None, Inline, Heap, External };

        BufferStorage() = default;
        ~BufferStorage() { Reset(); }

        BufferStorage(const BufferStorage &) = delete;
        BufferStorage &operator=(const BufferStorage &) = delete;

        BufferStorage(BufferStorage &&other) noexcept
            : m_Kind(other.m_Kind),
              m_Size(other.m_Size),
              m_Inline(other.m_Inline),
              m_Heap(std::move(other.m_Heap)),
              m_ExternalData(other.m_ExternalData),
              m_ExternalCleanup(other.m_ExternalCleanup),
              m_ExternalUserData(other.m_ExternalUserData) {
            other.m_Kind = Kind::None;
            other.m_Size = 0;
            other.m_ExternalData = nullptr;
            other.m_ExternalCleanup = nullptr;
            other.m_ExternalUserData = nullptr;
        }

        BufferStorage &operator=(BufferStorage &&other) noexcept {
            if (this == &other) {
                return *this;
            }

            Reset();
            m_Kind = other.m_Kind;
            m_Size = other.m_Size;
            m_Inline = other.m_Inline;
            m_Heap = std::move(other.m_Heap);
            m_ExternalData = other.m_ExternalData;
            m_ExternalCleanup = other.m_ExternalCleanup;
            m_ExternalUserData = other.m_ExternalUserData;

            other.m_Kind = Kind::None;
            other.m_Size = 0;
            other.m_ExternalData = nullptr;
            other.m_ExternalCleanup = nullptr;
            other.m_ExternalUserData = nullptr;
            return *this;
        }

        bool CopyFrom(const void *data, size_t size) {
            Reset();
            if (size == 0) {
                m_Size = 0;
                m_Kind = Kind::None;
                return true;
            }
            if (!data) {
                return false;
            }
            if (size <= m_Inline.size()) {
                std::memcpy(m_Inline.data(), data, size);
                m_Size = size;
                m_Kind = Kind::Inline;
                return true;
            }

            auto buffer = std::make_unique<uint8_t[]>(size);
            if (!buffer) {
                return false;
            }

            std::memcpy(buffer.get(), data, size);
            m_Size = size;
            m_Heap = std::move(buffer);
            m_Kind = Kind::Heap;
            return true;
        }

        bool Assign(const BML_ImcBuffer &buffer) {
            if (buffer.size > 0 && !buffer.data) {
                return false;
            }

            Reset();
            if (buffer.size == 0) {
                m_Size = 0;
                m_Kind = Kind::None;
                return true;
            }
            if (buffer.cleanup) {
                m_Size = buffer.size;
                m_Kind = Kind::External;
                m_ExternalData = buffer.data;
                m_ExternalCleanup = buffer.cleanup;
                m_ExternalUserData = buffer.cleanup_user_data;
                return true;
            }
            return CopyFrom(buffer.data, buffer.size);
        }

        const void *Data() const {
            switch (m_Kind) {
            case Kind::Inline:
                return m_Inline.data();
            case Kind::Heap:
                return m_Heap.get();
            case Kind::External:
                return m_ExternalData;
            case Kind::None:
            default:
                return nullptr;
            }
        }

        size_t Size() const { return m_Size; }

        void Reset() {
            if (m_Kind == Kind::External && m_ExternalCleanup) {
                m_ExternalCleanup(m_ExternalData, m_Size, m_ExternalUserData);
            }
            m_Heap.reset();
            m_ExternalData = nullptr;
            m_ExternalCleanup = nullptr;
            m_ExternalUserData = nullptr;
            m_Size = 0;
            m_Kind = Kind::None;
        }

    private:
        Kind m_Kind{Kind::None};
        size_t m_Size{0};
        std::array<uint8_t, kInlinePayloadBytes> m_Inline{};
        std::unique_ptr<uint8_t[]> m_Heap;
        const void *m_ExternalData{nullptr};
        void (*m_ExternalCleanup)(const void *, size_t, void *){nullptr};
        void *m_ExternalUserData{nullptr};
    };

    class TopicRegistry {
    public:
        BML_TopicId GetOrCreate(const char *name);
        const std::string *GetName(BML_TopicId id) const;
        void IncrementMessageCount(BML_TopicId id);
        std::atomic<uint64_t> *GetMessageCountPtr(BML_TopicId id);
        uint64_t GetMessageCount(BML_TopicId id) const;
        size_t GetTopicCount() const;

    private:
        struct TopicStats {
            std::atomic<uint64_t> message_count{0};
        };

        TopicStats *EnsureStatsEntryLocked(BML_TopicId id);

        mutable std::shared_mutex m_Mutex;
        std::unordered_map<std::string, BML_TopicId> m_NameToId;
        std::unordered_map<BML_TopicId, std::string> m_IdToName;
        std::unordered_map<BML_TopicId, std::unique_ptr<TopicStats>> m_Stats;
    };

    template <typename T>
    class PriorityMessageQueue {
    public:
        explicit PriorityMessageQueue(size_t capacity_per_level)
            : m_CapacityPerLevel(capacity_per_level) {
            for (size_t i = 0; i < kPriorityLevels; ++i) {
                m_Queues[i] = std::make_unique<MpscRingBuffer<T>>(capacity_per_level);
            }
            for (size_t i = 0; i < kPriorityLevels; ++i) {
                m_DrainCounter[i].store(0, std::memory_order_relaxed);
            }
        }

        bool Enqueue(T item, uint32_t priority) {
            size_t level = std::min(static_cast<size_t>(priority), kPriorityLevels - 1);
            return m_Queues[level] && m_Queues[level]->Enqueue(item);
        }

        bool Dequeue(T &out) {
            if (m_Queues[kPriorityLevels - 1] &&
                m_Queues[kPriorityLevels - 1]->Dequeue(out)) {
                IncrementDrainCounter(kPriorityLevels - 1);
                return true;
            }

            uint64_t total_high = GetTotalHighPriorityDrains();
            if (total_high > 0 && (total_high % 16) == 0) {
                if (m_Queues[0] && m_Queues[0]->Dequeue(out)) {
                    IncrementDrainCounter(0);
                    return true;
                }
                if (m_Queues[1] && m_Queues[1]->Dequeue(out)) {
                    IncrementDrainCounter(1);
                    return true;
                }
            }

            for (size_t i = kPriorityLevels - 1; i > 0; --i) {
                size_t level = i - 1;
                if (m_Queues[level] && m_Queues[level]->Dequeue(out)) {
                    IncrementDrainCounter(level);
                    return true;
                }
            }
            return false;
        }

        bool IsEmpty() const;
        size_t ApproximateSize() const;
        size_t Capacity() const;
        size_t GetLevelSize(uint32_t priority) const;

    private:
        void IncrementDrainCounter(size_t level) {
            m_DrainCounter[level].fetch_add(1, std::memory_order_relaxed);
        }

        uint64_t GetTotalHighPriorityDrains() const {
            return m_DrainCounter[2].load(std::memory_order_relaxed) +
                m_DrainCounter[3].load(std::memory_order_relaxed);
        }

        size_t m_CapacityPerLevel;
        std::unique_ptr<MpscRingBuffer<T>> m_Queues[kPriorityLevels];
        std::atomic<uint64_t> m_DrainCounter[kPriorityLevels];
    };

    class ImcBusImpl;
    Context *ContextFromKernel(KernelServices *kernel) noexcept;

    class ModuleContextScope {
    public:
        explicit ModuleContextScope(BML_Mod) {}
        ~ModuleContextScope() = default;

        ModuleContextScope(const ModuleContextScope &) = delete;
        ModuleContextScope &operator=(const ModuleContextScope &) = delete;
    };
} // namespace BML::Core

#endif // BML_CORE_IMC_BUS_SHARED_INTERNAL_H
