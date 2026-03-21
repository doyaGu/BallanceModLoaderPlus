/**
 * @file ImcBus.cpp
 * @brief Robust, high-performance IMC Bus implementation
 * 
 * Features:
 * - Lock-free MPSC queues with priority support
 * - Zero-copy buffer passing
 * - Per-subscription filtering and backpressure
 * - Comprehensive statistics collection
 * - Memory pool for allocation-free hot paths
 * - Topic name registry for debugging
 * 
 * API Count: 25
 */

#include "ImcBus.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <shared_mutex>
#include <thread>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ApiRegistry.h"
#include "ApiRegistrationMacros.h"
#include "Context.h"
#include "CrashDumpWriter.h"
#include "FaultTracker.h"
#include "FixedBlockPool.h"
#include "KernelServices.h"
#include "Logging.h"
#include "MpscRingBuffer.h"
#include "CoreErrors.h"

namespace BML::Core {
    void CoreLog(BML_LogSeverity level, const char *tag, const char *fmt, ...);
}

namespace {
    constexpr char kImcLogCategory[] = "imc.bus";
    constexpr size_t kDefaultQueueCapacity = 256;
    constexpr size_t kMaxQueueCapacity = 16384;
    constexpr size_t kInlinePayloadBytes = 256;
    constexpr size_t kDefaultRpcQueueCapacity = 256;
    constexpr size_t kPriorityLevels = 4; // LOW, NORMAL, HIGH, URGENT

    // Stack-allocated small vector. Avoids heap allocation for the common case
    // where subscriber count per topic is <= N. Falls back to heap for overflow.
    template <typename T, size_t N>
    class StackVec {
    public:
        StackVec() = default;
        StackVec(const StackVec &) = delete;
        StackVec &operator=(const StackVec &) = delete;

        void push_back(T value) {
            if (m_Size < N) {
                m_Inline[m_Size++] = value;
            } else {
                if (m_Size == N) {
                    m_Overflow.reserve(N * 2);
                    m_Overflow.assign(m_Inline, m_Inline + N);
                }
                m_Overflow.push_back(value);
                ++m_Size;
            }
        }

        void reserve(size_t) {} // no-op; inline buffer is fixed

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

    // ========================================================================
    // High-quality hash function (xxHash-inspired, but simpler)
    // ========================================================================

    uint32_t HashMix(uint32_t h) noexcept {
        h ^= h >> 16;
        h *= 0x85ebca6b;
        h ^= h >> 13;
        h *= 0xc2b2ae35;
        h ^= h >> 16;
        return h;
    }

    uint32_t ComputeId(const char *str) noexcept {
        // xxHash32-like algorithm for better distribution
        constexpr uint32_t PRIME1 = 0x9E3779B1u;
        constexpr uint32_t PRIME2 = 0x85EBCA77u;
        constexpr uint32_t PRIME3 = 0xC2B2AE3Du;

        uint32_t hash = 0x165667B1u; // Seed
        const char *p = str;

        while (*p) {
            hash += static_cast<uint32_t>(*p) * PRIME3;
            hash = ((hash << 17) | (hash >> 15)) * PRIME1;
            ++p;
        }

        hash ^= static_cast<uint32_t>(p - str);
        hash = HashMix(hash);

        return hash != 0 ? hash : 1u; // Ensure non-zero
    }

    // ========================================================================
    // High-precision timestamp
    // ========================================================================

    uint64_t GetTimestampNs() noexcept {
        using namespace std::chrono;
        return static_cast<uint64_t>(
            duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count()
        );
    }

    // ========================================================================
    // Internal Storage Types
    // ========================================================================

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
            if (!data)
                return false;
            if (size <= m_Inline.size()) {
                std::memcpy(m_Inline.data(), data, size);
                m_Size = size;
                m_Kind = Kind::Inline;
                return true;
            }
            auto buffer = std::make_unique<uint8_t[]>(size);
            if (!buffer)
                return false;
            std::memcpy(buffer.get(), data, size);
            m_Size = size;
            m_Heap = std::move(buffer);
            m_Kind = Kind::Heap;
            return true;
        }

        bool Assign(const BML_ImcBuffer &buffer) {
            if (buffer.size > 0 && !buffer.data)
                return false;
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

    struct QueuedMessage {
        BML_TopicId topic_id{0};
        BML_Mod sender{nullptr};
        uint64_t msg_id{0};
        uint32_t flags{0};
        uint32_t priority{BML_IMC_PRIORITY_NORMAL};
        uint64_t timestamp{0};
        BML_TopicId reply_topic{0};
        BufferStorage payload;
        std::atomic<uint32_t> ref_count{0};

        bool SetPayload(const void *data, size_t len) {
            return payload.CopyFrom(data, len);
        }

        bool SetExternalBuffer(const BML_ImcBuffer &buffer) {
            return payload.Assign(buffer);
        }

        const void *Data() const { return payload.Data(); }
        size_t Size() const { return payload.Size(); }
    };

    // ========================================================================
    // Topic Registry - maintains name-to-ID and ID-to-name mappings
    // ========================================================================

    class TopicRegistry {
    public:
        BML_TopicId GetOrCreate(const char *name) {
            if (!name || !*name)
                return BML_TOPIC_ID_INVALID;

            std::unique_lock lock(m_Mutex);
            std::string key{name};
            auto it = m_NameToId.find(key);
            if (it != m_NameToId.end()) {
                return it->second;
            }

            BML_TopicId id = ComputeId(key.c_str());
            while (m_IdToName.find(id) != m_IdToName.end()) {
                id = HashMix(id);
                if (id == 0) {
                    id = 1;
                }
            }

            m_IdToName.emplace(id, key);
            m_NameToId.emplace(std::move(key), id);
            EnsureStatsEntryLocked(id);
            return id;
        }

        const std::string *GetName(BML_TopicId id) const {
            std::shared_lock lock(m_Mutex);
            auto it = m_IdToName.find(id);
            return it != m_IdToName.end() ? &it->second : nullptr;
        }

        void IncrementMessageCount(BML_TopicId id) {
            if (id == BML_TOPIC_ID_INVALID)
                return;

            {
                std::shared_lock lock(m_Mutex);
                auto it = m_Stats.find(id);
                if (it != m_Stats.end()) {
                    it->second->message_count.fetch_add(1, std::memory_order_relaxed);
                    return;
                }
            }

            std::unique_lock lock(m_Mutex);
            auto stats = EnsureStatsEntryLocked(id);
            if (stats) {
                stats->message_count.fetch_add(1, std::memory_order_relaxed);
            }
        }

        /** @brief Get raw counter pointer for lock-free caching. Stable for topic lifetime. */
        std::atomic<uint64_t> *GetMessageCountPtr(BML_TopicId id) {
            std::shared_lock lock(m_Mutex);
            auto it = m_Stats.find(id);
            return it != m_Stats.end() ? &it->second->message_count : nullptr;
        }

        uint64_t GetMessageCount(BML_TopicId id) const {
            std::shared_lock lock(m_Mutex);
            auto it = m_Stats.find(id);
            return it != m_Stats.end() ? it->second->message_count.load(std::memory_order_relaxed) : 0;
        }

        size_t GetTopicCount() const {
            std::shared_lock lock(m_Mutex);
            return m_IdToName.size();
        }

    private:
        struct TopicStats {
            std::atomic<uint64_t> message_count{0};
        };

        TopicStats *EnsureStatsEntryLocked(BML_TopicId id) {
            if (id == BML_TOPIC_ID_INVALID)
                return nullptr;
            auto &entry = m_Stats[id];
            if (!entry) {
                entry = std::make_unique<TopicStats>();
            }
            return entry.get();
        }

        mutable std::shared_mutex m_Mutex;
        std::unordered_map<std::string, BML_TopicId> m_NameToId;
        std::unordered_map<BML_TopicId, std::string> m_IdToName;
        std::unordered_map<BML_TopicId, std::unique_ptr<TopicStats>> m_Stats;
    };

    // Global topic registry
    TopicRegistry &GetTopicRegistry() {
        static TopicRegistry registry;
        return registry;
    }

    // Similar for RPC
    TopicRegistry &GetRpcRegistry() {
        static TopicRegistry registry;
        return registry;
    }

    // ========================================================================
    // Priority Queue for Messages
    // ========================================================================

    /**
     * @brief Lock-free priority queue with starvation prevention
     *
     * Implements a 4-level priority system (LOW=0, NORMAL=1, HIGH=2, URGENT=3)
     * with a weighted fair queuing approach to prevent low-priority message starvation.
     *
     * ## Starvation Prevention Algorithm
     *
     * The dequeue algorithm uses a counter-based fairness mechanism:
     *
     * 1. **URGENT (level 3)**: Always processed first, no quota limit
     *    - Used for critical system messages that cannot be delayed
     *
     * 2. **HIGH (level 2)**: Processed after URGENT, participates in quota
     *    - Combined with URGENT for "high priority drain counter"
     *
     * 3. **NORMAL/LOW fairness**: Every 16 high-priority drains, the algorithm
     *    forces a LOW or NORMAL message to be processed (if available)
     *    - This guarantees low-priority messages are processed at least
     *      once per 16 high-priority messages
     *    - Prevents indefinite starvation under sustained high-priority load
     *
     * ## Performance Characteristics
     *
     * - Enqueue: O(1), delegates to underlying MpscRingBuffer
     * - Dequeue: O(1) amortized, checks levels in priority order
     * - Memory: O(capacity_per_level * 4 levels)
     *
     * @tparam T Message pointer type (typically QueuedMessage*)
     */
    template <typename T>
    class PriorityMessageQueue {
    public:
        explicit PriorityMessageQueue(size_t capacity_per_level)
            : m_CapacityPerLevel(capacity_per_level) {
            for (size_t i = 0; i < kPriorityLevels; ++i) {
                m_Queues[i] = std::make_unique<BML::Core::MpscRingBuffer<T>>(capacity_per_level);
            }
            // Reset drain counters
            for (size_t i = 0; i < kPriorityLevels; ++i) {
                m_DrainCounter[i].store(0, std::memory_order_relaxed);
            }
        }

        bool Enqueue(T item, uint32_t priority) {
            size_t level = std::min(static_cast<size_t>(priority), kPriorityLevels - 1);
            return m_Queues[level] && m_Queues[level]->Enqueue(item);
        }

        bool Dequeue(T &out) {
            // Weighted dequeue with starvation prevention:
            // - URGENT (3): Always try first, no limit
            // - HIGH (2): Try first unless we've drained 8+ consecutively without LOW/NORMAL
            // - NORMAL (1): Try if HIGH quota exhausted or empty
            // - LOW (0): Guaranteed 1 per 16 high-priority drains

            // First, try urgent - always prioritized
            if (m_Queues[kPriorityLevels - 1] && m_Queues[kPriorityLevels - 1]->Dequeue(out)) {
                IncrementDrainCounter(kPriorityLevels - 1);
                return true;
            }

            // Check if we need to service lower priority to prevent starvation
            // Every 16 high-priority drains, force a low-priority drain if available
            uint64_t total_high = GetTotalHighPriorityDrains();
            if (total_high > 0 && (total_high % 16) == 0) {
                // Try LOW first for fairness
                if (m_Queues[0] && m_Queues[0]->Dequeue(out)) {
                    IncrementDrainCounter(0);
                    return true;
                }
                // Then NORMAL
                if (m_Queues[1] && m_Queues[1]->Dequeue(out)) {
                    IncrementDrainCounter(1);
                    return true;
                }
            }

            // Standard priority order (HIGH -> NORMAL -> LOW)
            for (size_t i = kPriorityLevels - 1; i > 0; --i) {
                size_t level = i - 1;
                if (m_Queues[level] && m_Queues[level]->Dequeue(out)) {
                    IncrementDrainCounter(level);
                    return true;
                }
            }
            return false;
        }

        bool IsEmpty() const {
            for (size_t i = 0; i < kPriorityLevels; ++i) {
                if (m_Queues[i] && !m_Queues[i]->IsEmpty()) {
                    return false;
                }
            }
            return true;
        }

        size_t ApproximateSize() const {
            size_t total = 0;
            for (size_t i = 0; i < kPriorityLevels; ++i) {
                if (m_Queues[i]) {
                    total += m_Queues[i]->ApproximateSize();
                }
            }
            return total;
        }

        size_t Capacity() const {
            return m_CapacityPerLevel * kPriorityLevels;
        }

        size_t GetLevelSize(uint32_t priority) const {
            size_t level = std::min(static_cast<size_t>(priority), kPriorityLevels - 1);
            return m_Queues[level] ? m_Queues[level]->ApproximateSize() : 0;
        }

    private:
        void IncrementDrainCounter(size_t level) {
            m_DrainCounter[level].fetch_add(1, std::memory_order_relaxed);
        }

        uint64_t GetTotalHighPriorityDrains() const {
            // HIGH and URGENT combined
            return m_DrainCounter[2].load(std::memory_order_relaxed) +
                m_DrainCounter[3].load(std::memory_order_relaxed);
        }

        size_t m_CapacityPerLevel;
        std::unique_ptr<BML::Core::MpscRingBuffer<T>> m_Queues[kPriorityLevels];
        std::atomic<uint64_t> m_DrainCounter[kPriorityLevels];
    };
} // namespace (anonymous) - close for global-scope handle types

// ========================================================================
// Subscription Internal Type (global scope to match bml_types.h forward decl)
// ========================================================================

struct BML_Subscription_T {
    // Core fields
    BML_TopicId topic_id{0};
    BML_ImcHandler handler{nullptr};
    BML_ImcInterceptHandler intercept_handler{nullptr};
    void *user_data{nullptr};
    BML_Mod owner{nullptr};
    std::atomic<uint32_t> ref_count{0};
    std::atomic<bool> closed{false};

    // Extended options
    size_t queue_capacity{kDefaultQueueCapacity};
    uint32_t min_priority{BML_IMC_PRIORITY_LOW};
    uint32_t backpressure_policy{BML_BACKPRESSURE_FAIL}; // Default to FAIL for explicit error handling
    int32_t execution_order{0};
    BML_ImcFilter filter{nullptr};
    void *filter_user_data{nullptr};

    bool IsIntercept() const { return intercept_handler != nullptr; }

    // Priority queue
    std::unique_ptr<PriorityMessageQueue<QueuedMessage *>> queue;

    // Statistics (atomic for lock-free reads)
    std::atomic<uint64_t> messages_received{0};
    std::atomic<uint64_t> messages_processed{0};
    std::atomic<uint64_t> messages_dropped{0};
    std::atomic<uint64_t> total_bytes_received{0};
    std::atomic<uint64_t> last_message_time{0};
    std::atomic<uint64_t> creation_time{0};

    void InitStats() {
        creation_time.store(GetTimestampNs(), std::memory_order_relaxed);
    }

    void RecordReceived(size_t bytes) {
        messages_received.fetch_add(1, std::memory_order_relaxed);
        total_bytes_received.fetch_add(bytes, std::memory_order_relaxed);
        last_message_time.store(GetTimestampNs(), std::memory_order_relaxed);
    }

    void RecordProcessed() {
        messages_processed.fetch_add(1, std::memory_order_relaxed);
    }

    void RecordDropped() {
        messages_dropped.fetch_add(1, std::memory_order_relaxed);
    }

    void FillStats(BML_SubscriptionStats *stats) const {
        if (!stats) return;
        stats->struct_size = sizeof(BML_SubscriptionStats);
        stats->messages_received = messages_received.load(std::memory_order_relaxed);
        stats->messages_processed = messages_processed.load(std::memory_order_relaxed);
        stats->messages_dropped = messages_dropped.load(std::memory_order_relaxed);
        stats->total_bytes = total_bytes_received.load(std::memory_order_relaxed);
        stats->queue_size = queue ? queue->ApproximateSize() : 0;
        stats->queue_capacity = queue_capacity * kPriorityLevels;
        stats->last_message_time = last_message_time.load(std::memory_order_relaxed);
    }
};

thread_local std::vector<BML_Subscription_T *> g_DispatchSubscriptionStack;

class DispatchSubscriptionScope {
public:
    explicit DispatchSubscriptionScope(BML_Subscription_T *sub) {
        constexpr size_t kMaxDispatchDepth = 64;
        if (g_DispatchSubscriptionStack.size() >= kMaxDispatchDepth) {
            BML::Core::CoreLog(BML_LOG_ERROR, kImcLogCategory,
                               "IMC dispatch stack overflow (depth %zu)",
                               g_DispatchSubscriptionStack.size());
            m_Overflow = true;
            return;
        }
        g_DispatchSubscriptionStack.push_back(sub);
        m_Overflow = false;
    }

    ~DispatchSubscriptionScope() {
        if (!m_Overflow)
            g_DispatchSubscriptionStack.pop_back();
    }

    bool Overflowed() const { return m_Overflow; }

    DispatchSubscriptionScope(const DispatchSubscriptionScope &) = delete;
    DispatchSubscriptionScope &operator=(const DispatchSubscriptionScope &) = delete;

private:
    bool m_Overflow = false;
};

// ========================================================================
// Future Internal Type
// ========================================================================

struct FutureCallbackEntry {
    BML_FutureCallback fn{nullptr};
    void *user_data{nullptr};
};

struct BML_Future_T {
    std::atomic<uint32_t> ref_count{1};
    std::mutex mutex;
    std::condition_variable cv;
    BML_FutureState state{BML_FUTURE_PENDING};
    BML_Result status{BML_RESULT_OK};
    BufferStorage payload;
    uint64_t msg_id{0};
    uint32_t flags{0};
    uint64_t creation_time{0};
    uint64_t completion_time{0};
    std::string error_message;  // populated on failure if handler provides one
    std::vector<FutureCallbackEntry> callbacks;

    BML_Future_T() : creation_time(GetTimestampNs()) {}

    void Complete(BML_FutureState new_state,
                  BML_Result new_status,
                  const void *data,
                  size_t size) {
        std::vector<FutureCallbackEntry> pending_callbacks;
        {
            std::unique_lock lock(mutex);
            if (state != BML_FUTURE_PENDING)
                return;
            state = new_state;
            status = new_status;
            completion_time = GetTimestampNs();
            if (new_state == BML_FUTURE_READY && new_status == BML_RESULT_OK && data && size > 0) {
                if (!payload.CopyFrom(data, size)) {
                    status = BML_RESULT_OUT_OF_MEMORY;
                    state = BML_FUTURE_FAILED;
                }
            }
            pending_callbacks = callbacks;
            callbacks.clear();
        }
        NotifyCallbacks(std::move(pending_callbacks));
    }

    void CompleteWithError(BML_FutureState new_state,
                           BML_Result new_status,
                           const char *err_msg) {
        std::vector<FutureCallbackEntry> pending_callbacks;
        {
            std::unique_lock lock(mutex);
            if (state != BML_FUTURE_PENDING)
                return;
            state = new_state;
            status = new_status;
            completion_time = GetTimestampNs();
            if (err_msg && *err_msg)
                error_message = err_msg;
            pending_callbacks = callbacks;
            callbacks.clear();
        }
        NotifyCallbacks(std::move(pending_callbacks));
    }

    bool Cancel() {
        std::vector<FutureCallbackEntry> pending_callbacks;
        {
            std::unique_lock lock(mutex);
            if (state != BML_FUTURE_PENDING)
                return false;
            state = BML_FUTURE_CANCELLED;
            status = BML_RESULT_FAIL;
            completion_time = GetTimestampNs();
            pending_callbacks = callbacks;
            callbacks.clear();
        }
        NotifyCallbacks(std::move(pending_callbacks));
        return true;
    }

    void NotifyCallbacks(std::vector<FutureCallbackEntry> &&pending_callbacks) {
        cv.notify_all();
        BML_Context ctx = BML::Core::GetKernelOrNull()->context->GetHandle();
        for (const auto &entry : pending_callbacks) {
            if (entry.fn)
                entry.fn(ctx, this, entry.user_data);
        }
    }

    uint64_t GetLatencyNs() const {
        if (completion_time == 0) return 0;
        return completion_time - creation_time;
    }
};

// ========================================================================
// RPC Stream Internal Type
// ========================================================================

struct BML_RpcStream_T {
    BML_Future future{nullptr};
    BML_TopicId chunk_topic{BML_TOPIC_ID_INVALID};
    BML_ImcHandler on_chunk{nullptr};
    BML_FutureCallback on_done{nullptr};
    void *user_data{nullptr};
    BML_Mod owner{nullptr};
    std::atomic<bool> completed{false};
};

static BML_Future CreateFuture() {
    return new(std::nothrow) BML_Future_T();
}

static void FutureAddRefInternal(BML_Future future) {
    if (future)
        future->ref_count.fetch_add(1, std::memory_order_relaxed);
}

static void FutureReleaseInternal(BML_Future future) {
    if (!future)
        return;
    uint32_t previous = future->ref_count.fetch_sub(1, std::memory_order_acq_rel);
    if (previous == 1)
        delete future;
}

namespace {
    // ========================================================================
    // RPC Types (internal)
    // ========================================================================

    struct RpcHandlerStats {
        std::atomic<uint64_t> call_count{0};
        std::atomic<uint64_t> completion_count{0};
        std::atomic<uint64_t> failure_count{0};
        std::atomic<uint64_t> total_latency_ns{0};
    };

    struct RpcHandlerEntry {
        BML_RpcHandler handler{nullptr};
        BML_RpcHandlerEx handler_ex{nullptr};
        void *user_data{nullptr};
        BML_Mod owner{nullptr};
        std::shared_ptr<RpcHandlerStats> stats = std::make_shared<RpcHandlerStats>();
    };

    struct RpcRequest {
        BML_RpcId rpc_id{0};
        BufferStorage payload;
        uint64_t msg_id{0};
        BML_Mod caller{nullptr};
        BML_Future future{nullptr};
        uint64_t deadline_ns{0};   // 0 = no timeout
    };

    struct RpcMiddlewareEntry {
        BML_RpcMiddleware middleware{nullptr};
        int32_t priority{0};
        void *user_data{nullptr};
        BML_Mod owner{nullptr};
    };

    struct StreamingRpcHandlerEntry {
        BML_StreamingRpcHandler handler{nullptr};
        void *user_data{nullptr};
        BML_Mod owner{nullptr};
    };

    struct RetainedStateEntry {
        BML_Mod owner{nullptr};
        uint64_t timestamp{0};
        uint32_t flags{0};
        uint32_t priority{BML_IMC_PRIORITY_NORMAL};
        BufferStorage payload;
    };
} // namespace (anonymous)

namespace BML::Core {
    // BML_Future_T and BML_Subscription_T are now in global scope - no import needed
    // Other types from anonymous namespace are accessible via unqualified lookup in same TU

    namespace {

#if defined(_MSC_VER) && !defined(__MINGW32__)
        static unsigned long InvokeHandlerSEH(BML_ImcHandler handler,
                                              BML_Context ctx,
                                              BML_TopicId topic_id,
                                              BML_ImcMessage *imc_msg,
                                              void *user_data);
        static unsigned long InvokeInterceptHandlerSEH(BML_ImcInterceptHandler handler,
                                                       BML_Context ctx,
                                                       BML_TopicId topic_id,
                                                       BML_ImcMessage *imc_msg,
                                                       void *user_data,
                                                       BML_EventResult *out_result);
#endif

        // ========================================================================
        // TopicSnapshot - Immutable snapshot for RCU-style lock-free reads
        // ========================================================================

        struct TopicEntry {
            std::vector<BML_Subscription_T *> subs;
            std::atomic<uint64_t> *message_counter = nullptr; // Cached; stable for topic lifetime
        };

        struct TopicSnapshot {
            std::unordered_map<BML_TopicId, TopicEntry> topic_subs;
            std::vector<BML_Subscription_T *> all_subs;
            std::atomic<uint32_t> ref_count{0};

            TopicSnapshot() = default;

            // Non-copyable, non-movable (due to atomic)
            TopicSnapshot(const TopicSnapshot &) = delete;
            TopicSnapshot &operator=(const TopicSnapshot &) = delete;
        };

        // ========================================================================
        // RAII guard for snapshot reference counting
        // ========================================================================

        class SnapshotGuard {
        public:
            explicit SnapshotGuard(TopicSnapshot *snap) noexcept : m_Snap(snap) {
                if (m_Snap)
                    m_Snap->ref_count.fetch_add(1, std::memory_order_acq_rel);
            }
            ~SnapshotGuard() {
                if (m_Snap)
                    m_Snap->ref_count.fetch_sub(1, std::memory_order_acq_rel);
            }
            SnapshotGuard(const SnapshotGuard &) = delete;
            SnapshotGuard &operator=(const SnapshotGuard &) = delete;

            TopicSnapshot *get() const noexcept { return m_Snap; }
            explicit operator bool() const noexcept { return m_Snap != nullptr; }
        private:
            TopicSnapshot *m_Snap;
        };

        // ImcBusImpl - Core Implementation
        // ========================================================================

        class ImcBusImpl {
        public:
            ImcBusImpl();

            // ID Resolution
            BML_Result GetTopicId(const char *name, BML_TopicId *out_id);
            BML_Result GetRpcId(const char *name, BML_RpcId *out_id);

            // Pub/Sub (Core)
            BML_Result Publish(BML_TopicId topic, const void *data, size_t size);
            BML_Result PublishEx(BML_TopicId topic, const BML_ImcMessage *msg);
            BML_Result PublishBuffer(BML_TopicId topic, const BML_ImcBuffer *buffer);
            BML_Result Subscribe(BML_TopicId topic, BML_ImcHandler handler, void *user_data, BML_Subscription *out_sub);
            BML_Result Unsubscribe(BML_Subscription sub);
            BML_Result SubscriptionIsActive(BML_Subscription sub, BML_Bool *out_active);

            // Pub/Sub (Extended)
            BML_Result SubscribeEx(BML_TopicId topic, BML_ImcHandler handler, void *user_data,
                                   const BML_SubscribeOptions *options, BML_Subscription *out_sub);
            BML_Result SubscribeIntercept(BML_TopicId topic, BML_ImcInterceptHandler handler,
                                          void *user_data, BML_Subscription *out_sub);
            BML_Result SubscribeInterceptEx(BML_TopicId topic, BML_ImcInterceptHandler handler,
                                            void *user_data, const BML_SubscribeOptions *options,
                                            BML_Subscription *out_sub);
            BML_Result PublishInterceptable(BML_TopicId topic, BML_ImcMessage *msg,
                                            BML_EventResult *out_result);
            BML_Result GetSubscriptionStats(BML_Subscription sub, BML_SubscriptionStats *out_stats);
            BML_Result PublishMulti(const BML_TopicId *topics, size_t topic_count,
                                    const void *data, size_t size, const BML_ImcMessage *msg,
                                    size_t *out_delivered);

            // RPC
            BML_Result RegisterRpc(BML_RpcId rpc_id, BML_RpcHandler handler, void *user_data);
            BML_Result UnregisterRpc(BML_RpcId rpc_id);
            BML_Result CallRpc(BML_RpcId rpc_id, const BML_ImcMessage *request, BML_Future *out_future);

            // RPC v1.1
            BML_Result RegisterRpcEx(BML_RpcId rpc_id, BML_RpcHandlerEx handler, void *user_data);
            BML_Result CallRpcEx(BML_RpcId rpc_id, const BML_ImcMessage *request, const BML_RpcCallOptions *options, BML_Future *out_future);
            BML_Result FutureGetError(BML_Future future, BML_Result *out_code, char *msg, size_t cap, size_t *out_len);
            BML_Result GetRpcInfo(BML_RpcId rpc_id, BML_RpcInfo *out_info);
            BML_Result GetRpcName(BML_RpcId rpc_id, char *buf, size_t cap, size_t *out_len);
            void EnumerateRpc(void(*cb)(BML_RpcId, const char *, BML_Bool, void *), void *user_data);
            BML_Result AddRpcMiddleware(BML_RpcMiddleware middleware, int32_t priority, void *user_data);
            BML_Result RemoveRpcMiddleware(BML_RpcMiddleware middleware);
            BML_Result RegisterStreamingRpc(BML_RpcId rpc_id, BML_StreamingRpcHandler handler, void *user_data);
            BML_Result StreamPush(BML_RpcStream stream, const void *data, size_t size);
            BML_Result StreamComplete(BML_RpcStream stream);
            BML_Result StreamError(BML_RpcStream stream, BML_Result error, const char *msg);
            BML_Result CallStreamingRpc(BML_RpcId rpc_id, const BML_ImcMessage *request,
                                        BML_ImcHandler on_chunk, BML_FutureCallback on_done,
                                        void *user_data, BML_Future *out_future);

            // Futures
            BML_Result FutureAwait(BML_Future future, uint32_t timeout_ms);
            BML_Result FutureGetResult(BML_Future future, BML_ImcMessage *out_msg);
            BML_Result FutureGetState(BML_Future future, BML_FutureState *out_state);
            BML_Result FutureCancel(BML_Future future);
            BML_Result FutureOnComplete(BML_Future future, BML_FutureCallback callback, void *user_data);
            BML_Result FutureRelease(BML_Future future);

            // Statistics & Diagnostics
            BML_Result GetStats(BML_ImcStats *out_stats);
            BML_Result ResetStats();
            BML_Result GetTopicInfo(BML_TopicId topic, BML_TopicInfo *out_info);
            BML_Result GetTopicName(BML_TopicId topic, char *buffer, size_t buffer_size, size_t *out_length);
            BML_Result PublishState(BML_TopicId topic, const BML_ImcMessage *msg);
            BML_Result CopyState(BML_TopicId topic,
                                 void *dst,
                                 size_t dst_size,
                                 size_t *out_size,
                                 BML_ImcStateMeta *out_meta);
            BML_Result ClearState(BML_TopicId topic);

            // Pump
            void Pump(size_t max_per_sub);
            void Shutdown();
            void CleanupOwner(BML_Mod owner);

        private:
            using SubscriptionPtr = BML_Subscription;

            QueuedMessage *CreateMessage(BML_TopicId topic, const void *data, size_t size,
                                         const BML_ImcMessage *msg, const BML_ImcBuffer *buffer);
            BML_Result DispatchMessage(BML_TopicId topic, QueuedMessage *message);
            BML_Result DispatchToSubscription(BML_Subscription_T *sub, QueuedMessage *message, bool *out_enqueued);
            bool MatchesSubscription(BML_Subscription_T *sub, const BML_ImcMessage &message) const;
            void InvokeRegularHandler(BML_Subscription_T *sub, BML_TopicId topic, const BML_ImcMessage &message);
            void ReleaseMessage(QueuedMessage *message);
            void DropPendingMessages(BML_Subscription_T *sub);
            size_t DrainSubscription(BML_Subscription_T *sub, size_t budget);
            void RemoveFromMutableTopicMap(BML_TopicId topic, SubscriptionPtr handle);
            void CleanupRetiredSubscriptions();
            void ProcessRpcRequest(RpcRequest *request);
            void DrainRpcQueue(size_t budget);
            void ApplyBackpressure(BML_Subscription_T *sub, QueuedMessage *message);
            BML_Mod ResolveRegistrationOwner() const;
            BML_Result ResolveRegistrationOwner(BML_Mod *out_owner) const;

            // RCU snapshot management (called under m_WriteMutex)
            void PublishNewSnapshot();
            void RetireOldSnapshots();

            // Write-side mutex (Subscribe/Unsubscribe only, infrequent)
            std::mutex m_WriteMutex;
            // Lock-free read snapshot (Publish/Pump read atomically)
            std::atomic<TopicSnapshot *> m_Snapshot{nullptr};
            // Old snapshots pending retirement
            std::vector<TopicSnapshot *> m_RetiredSnapshots;

            mutable std::shared_mutex m_RpcMutex;

            // Mutable topic map (only accessed under m_WriteMutex)
            std::unordered_map<BML_TopicId, std::vector<SubscriptionPtr>> m_MutableTopicMap;
            // Subscription ownership (only mutated under m_WriteMutex)
            std::unordered_map<SubscriptionPtr, std::unique_ptr<BML_Subscription_T>> m_Subscriptions;
            std::vector<std::unique_ptr<BML_Subscription_T>> m_RetiredSubscriptions;

            // RPC handlers (ID-based only)
            std::unordered_map<BML_RpcId, RpcHandlerEntry> m_RpcHandlers;
            std::vector<RpcMiddlewareEntry> m_RpcMiddleware;
            std::unordered_map<BML_RpcId, StreamingRpcHandlerEntry> m_StreamingRpcHandlers;
            std::unordered_map<BML_RpcStream, std::unique_ptr<BML_RpcStream_T>> m_Streams;
            std::mutex m_StreamMutex;
            std::mutex m_StateMutex;
            std::unordered_map<BML_TopicId, RetainedStateEntry> m_RetainedStates;

            FixedBlockPool m_MessagePool;
            FixedBlockPool m_RpcRequestPool;
            std::unique_ptr<MpscRingBuffer<RpcRequest *>> m_RpcQueue;
            std::atomic<uint64_t> m_NextMessageId{1};

            // Global Statistics (atomic for lock-free access)
            struct GlobalStats {
                std::atomic<uint64_t> total_messages_published{0};
                std::atomic<uint64_t> total_messages_delivered{0};
                std::atomic<uint64_t> total_messages_dropped{0};
                std::atomic<uint64_t> total_bytes_published{0};
                std::atomic<uint64_t> total_rpc_calls{0};
                std::atomic<uint64_t> total_rpc_completions{0};
                std::atomic<uint64_t> total_rpc_failures{0};
                std::atomic<uint64_t> pump_cycles{0};
                std::atomic<uint64_t> last_pump_time{0};
                std::atomic<uint64_t> start_time{0};

                GlobalStats() {
                    start_time.store(GetTimestampNs(), std::memory_order_relaxed);
                }

                void Reset() {
                    total_messages_published.store(0, std::memory_order_relaxed);
                    total_messages_delivered.store(0, std::memory_order_relaxed);
                    total_messages_dropped.store(0, std::memory_order_relaxed);
                    total_bytes_published.store(0, std::memory_order_relaxed);
                    total_rpc_calls.store(0, std::memory_order_relaxed);
                    total_rpc_completions.store(0, std::memory_order_relaxed);
                    total_rpc_failures.store(0, std::memory_order_relaxed);
                    pump_cycles.store(0, std::memory_order_relaxed);
                    start_time.store(GetTimestampNs(), std::memory_order_relaxed);
                }
            };

            GlobalStats m_Stats;
        };

        ImcBusImpl::ImcBusImpl()
            : m_MessagePool(sizeof(QueuedMessage)),
              m_RpcRequestPool(sizeof(RpcRequest)),
              m_RpcQueue(std::make_unique<MpscRingBuffer<RpcRequest *>>(kDefaultRpcQueueCapacity)) {
            // Start with an empty snapshot for lock-free reads
            m_Snapshot.store(new TopicSnapshot(), std::memory_order_release);
        }

        // Set by ImcBus constructor, cleared by destructor.
        ImcBusImpl *g_BusPtr = nullptr;

        ImcBusImpl &GetBus() {
            if (g_BusPtr)
                return *g_BusPtr;
            // Fallback for pre-KernelServices usage (shouldn't happen in practice).
            static ImcBusImpl fallback;
            return fallback;
        }

        BML_Mod ImcBusImpl::ResolveRegistrationOwner() const {
            auto current = Context::GetCurrentModule();
            if (!current) {
                return nullptr;
            }
            return GetKernelOrNull()->context->ResolveModHandle(current);
        }

        BML_Result ImcBusImpl::ResolveRegistrationOwner(BML_Mod *out_owner) const {
            if (!out_owner) {
                return BML_RESULT_INVALID_ARGUMENT;
            }

            *out_owner = ResolveRegistrationOwner();
            return *out_owner ? BML_RESULT_OK : BML_RESULT_INVALID_CONTEXT;
        }

        // ========================================================================
        // ID Resolution
        // ========================================================================

        BML_Result ImcBusImpl::GetTopicId(const char *name, BML_TopicId *out_id) {
            if (!name || !*name || !out_id)
                return BML_RESULT_INVALID_ARGUMENT;
            *out_id = GetTopicRegistry().GetOrCreate(name);
            return (*out_id != BML_TOPIC_ID_INVALID) ? BML_RESULT_OK : BML_RESULT_FAIL;
        }

        BML_Result ImcBusImpl::GetRpcId(const char *name, BML_RpcId *out_id) {
            if (!name || !*name || !out_id)
                return BML_RESULT_INVALID_ARGUMENT;
            *out_id = GetRpcRegistry().GetOrCreate(name);
            return (*out_id != BML_RPC_ID_INVALID) ? BML_RESULT_OK : BML_RESULT_FAIL;
        }

        // ========================================================================
        // Message Handling
        // ========================================================================

        QueuedMessage *ImcBusImpl::CreateMessage(BML_TopicId topic, const void *data, size_t size,
                                                 const BML_ImcMessage *msg, const BML_ImcBuffer *buffer) {
            auto *message = m_MessagePool.Construct<QueuedMessage>();
            if (!message)
                return nullptr;

            message->topic_id = topic;
            message->sender = Context::GetCurrentModule();
            message->msg_id = msg && msg->msg_id ? msg->msg_id : m_NextMessageId.fetch_add(1, std::memory_order_relaxed);
            message->flags = msg ? msg->flags : 0;
            message->priority = msg ? msg->priority : BML_IMC_PRIORITY_NORMAL;
            message->timestamp = (msg && msg->timestamp) ? msg->timestamp : GetTimestampNs();
            message->reply_topic = msg ? msg->reply_topic : 0;

            bool ok = buffer ? message->SetExternalBuffer(*buffer) : message->SetPayload(data, size);
            if (!ok) {
                m_MessagePool.Destroy(message);
                return nullptr;
            }

            // Update global stats
            m_Stats.total_messages_published.fetch_add(1, std::memory_order_relaxed);
            m_Stats.total_bytes_published.fetch_add(message->Size(), std::memory_order_relaxed);

            return message;
        }

        void ImcBusImpl::ApplyBackpressure(BML_Subscription_T *sub, QueuedMessage *message) {
            if (!sub || !sub->queue || !message)
                return;

            switch (sub->backpressure_policy) {
            case BML_BACKPRESSURE_DROP_OLDEST: {
                // Try to drop oldest message to make room
                QueuedMessage *old = nullptr;
                if (sub->queue->Dequeue(old)) {
                    ReleaseMessage(old);
                    sub->RecordDropped();
                    m_Stats.total_messages_dropped.fetch_add(1, std::memory_order_relaxed);
                }
                break;
            }
            case BML_BACKPRESSURE_DROP_NEWEST:
                // Just drop the new message (caller handles this)
                break;
            case BML_BACKPRESSURE_BLOCK:
                // In a real implementation, this could spin or yield
                // For now, we just drop and record
                break;
            case BML_BACKPRESSURE_FAIL:
            default:
                // Fail silently, caller handles
                break;
            }
        }

        // Returns OK for accepted, filtered, or dropped messages.
        // Returns WOULD_BLOCK only for explicit FAIL backpressure.
        BML_Result ImcBusImpl::DispatchToSubscription(BML_Subscription_T *sub, QueuedMessage *message,
                                                      bool *out_enqueued) {
            if (out_enqueued) {
                *out_enqueued = false;
            }
            if (!sub || !message)
                return BML_RESULT_INVALID_ARGUMENT;

            BML_ImcMessage filter_msg = {};
            filter_msg.struct_size = sizeof(BML_ImcMessage);
            filter_msg.data = message->Data();
            filter_msg.size = message->Size();
            filter_msg.msg_id = message->msg_id;
            filter_msg.flags = message->flags;
            filter_msg.priority = message->priority;
            filter_msg.timestamp = message->timestamp;
            filter_msg.reply_topic = message->reply_topic;

            if (!MatchesSubscription(sub, filter_msg))
                return BML_RESULT_OK;
 
            if (sub->queue && sub->queue->Enqueue(message, message->priority)) {
                sub->RecordReceived(message->Size());
                m_Stats.total_messages_delivered.fetch_add(1, std::memory_order_relaxed);
                if (out_enqueued) {
                    *out_enqueued = true;
                }
                return BML_RESULT_OK;
            }

            // Queue full - apply backpressure policy
            switch (sub->backpressure_policy) {
            case BML_BACKPRESSURE_DROP_OLDEST:
                ApplyBackpressure(sub, message);
                if (sub->queue && sub->queue->Enqueue(message, message->priority)) {
                    sub->RecordReceived(message->Size());
                    m_Stats.total_messages_delivered.fetch_add(1, std::memory_order_relaxed);
                    if (out_enqueued) {
                        *out_enqueued = true;
                    }
                    return BML_RESULT_OK;
                }
                break;

            case BML_BACKPRESSURE_DROP_NEWEST:
                sub->RecordDropped();
                m_Stats.total_messages_dropped.fetch_add(1, std::memory_order_relaxed);
                return BML_RESULT_OK;

            case BML_BACKPRESSURE_BLOCK:
                for (int spin = 0; spin < 100; ++spin) {
                    std::this_thread::yield();
                    if (sub->queue && sub->queue->Enqueue(message, message->priority)) {
                        sub->RecordReceived(message->Size());
                        m_Stats.total_messages_delivered.fetch_add(1, std::memory_order_relaxed);
                        if (out_enqueued) {
                            *out_enqueued = true;
                        }
                        return BML_RESULT_OK;
                    }
                }
                break;

            case BML_BACKPRESSURE_FAIL:
                return BML_RESULT_WOULD_BLOCK;
            default:
                break;
            }

            sub->RecordDropped();
            m_Stats.total_messages_dropped.fetch_add(1, std::memory_order_relaxed);
            return BML_RESULT_OK;
        }

        bool ImcBusImpl::MatchesSubscription(BML_Subscription_T *sub, const BML_ImcMessage &message) const {
            if (!sub)
                return false;

            if (message.priority < sub->min_priority)
                return false;

            if (sub->filter && sub->filter(&message, sub->filter_user_data) != BML_TRUE)
                return false;

            return true;
        }

        void ImcBusImpl::InvokeRegularHandler(BML_Subscription_T *sub,
                                             BML_TopicId topic,
                                             const BML_ImcMessage &message) {
            if (!sub || sub->closed.load(std::memory_order_acquire) || !sub->handler)
                return;

            BML_Context ctx = GetKernelOrNull()->context->GetHandle();

            try {
                DispatchSubscriptionScope dispatch_scope(sub);
                if (dispatch_scope.Overflowed())
                    return;
#if defined(_MSC_VER) && !defined(__MINGW32__)
                BML_ImcMessage mutable_message = message;
                unsigned long seh_code = InvokeHandlerSEH(
                    sub->handler, ctx, topic, &mutable_message, sub->user_data);
                if (seh_code != 0) {
                    std::string owner_id;
                    if (sub->owner) {
                        auto *mod = GetKernelOrNull()->context->ResolveModHandle(sub->owner);
                        if (mod) owner_id = mod->id;
                    }
                    CoreLog(BML_LOG_ERROR, kImcLogCategory,
                            "Subscriber crashed (code 0x%08lX) on topic %u, "
                            "owned by module '%s' -- unsubscribing",
                            seh_code, static_cast<unsigned>(topic),
                            owner_id.empty() ? "unknown" : owner_id.c_str());

                    GetKernelOrNull()->crash_dump->WriteDumpOnce(owner_id, seh_code);
                    if (!owner_id.empty()) {
                        GetKernelOrNull()->fault_tracker->RecordFault(owner_id, seh_code);
                    }
                    sub->closed.store(true, std::memory_order_release);
                } else {
                    sub->RecordProcessed();
                }
#else
                sub->handler(ctx, topic, &message, sub->user_data);
                sub->RecordProcessed();
#endif
            } catch (...) {
                // C++ exception -- log and continue
            }
        }

        BML_Result ImcBusImpl::DispatchMessage(BML_TopicId topic, QueuedMessage *message) {
            if (!message)
                return BML_RESULT_OUT_OF_MEMORY;

            // Lock-free snapshot read (RCU pattern)
            SnapshotGuard guard(m_Snapshot.load(std::memory_order_acquire));
            if (!guard) {
                GetTopicRegistry().IncrementMessageCount(topic);
                ReleaseMessage(message);
                return BML_RESULT_OK;
            }

            // Single lookup: find the TopicEntry (subs + cached stats counter)
            auto it = guard.get()->topic_subs.find(topic);
            if (it == guard.get()->topic_subs.end() || it->second.subs.empty()) {
                GetTopicRegistry().IncrementMessageCount(topic);
                ReleaseMessage(message);
                return BML_RESULT_OK;
            }

            const auto &entry = it->second;

            // Lock-free stats increment via cached pointer
            if (entry.message_counter) {
                entry.message_counter->fetch_add(1, std::memory_order_relaxed);
            } else {
                GetTopicRegistry().IncrementMessageCount(topic);
            }

            // First pass: collect valid targets (inline for <= 8 subscribers)
            StackVec<BML_Subscription_T *, 8> targets;
            for (auto *sub : entry.subs) {
                if (sub->closed.load(std::memory_order_acquire) || !sub->handler || !sub->queue)
                    continue;
                targets.push_back(sub);
            }

            if (targets.empty()) {
                ReleaseMessage(message);
                return BML_RESULT_OK;
            }

            // THREAD SAFETY: ref_count is set before dispatch; snapshot guard
            // keeps the snapshot alive so subscription pointers remain valid.
            // Unsubscribe waits for sub->ref_count == 0 before destroying.
            message->ref_count.store(static_cast<uint32_t>(targets.size()) + 1, std::memory_order_release);

            for (auto *sub : targets) {
                sub->ref_count.fetch_add(1, std::memory_order_relaxed);
            }

            uint32_t delivered = 0;
            BML_Result first_error = BML_RESULT_OK;
            for (auto *sub : targets) {
                bool enqueued = false;
                const BML_Result dispatch_result = DispatchToSubscription(sub, message, &enqueued);
                if (dispatch_result == BML_RESULT_OK && enqueued) {
                    ++delivered;
                } else {
                    ReleaseMessage(message);
                    if (dispatch_result != BML_RESULT_OK && first_error == BML_RESULT_OK) {
                        first_error = dispatch_result;
                    }
                }
                sub->ref_count.fetch_sub(1, std::memory_order_acq_rel);
            }

            ReleaseMessage(message); // Release our own reference
            if (delivered == 0 && first_error != BML_RESULT_OK) {
                return first_error;
            }
            return BML_RESULT_OK;
        }

        void ImcBusImpl::ReleaseMessage(QueuedMessage *message) {
            if (!message)
                return;
            uint32_t prev = message->ref_count.fetch_sub(1, std::memory_order_acq_rel);
            if (prev == 1)
                m_MessagePool.Destroy(message);
        }

        void ImcBusImpl::DropPendingMessages(BML_Subscription_T *sub) {
            if (!sub || !sub->queue)
                return;
            QueuedMessage *msg = nullptr;
            while (sub->queue->Dequeue(msg)) {
                ReleaseMessage(msg);
            }
        }

        // SEH-safe handler invocation helpers. These functions have no C++
        // destructors on the stack, which is required for __try/__except on MSVC.
#if defined(_MSC_VER) && !defined(__MINGW32__)
        static int FilterModuleException(unsigned long code) {
            switch (code) {
                case EXCEPTION_ACCESS_VIOLATION:
                case EXCEPTION_ILLEGAL_INSTRUCTION:
                case EXCEPTION_STACK_OVERFLOW:
                case EXCEPTION_INT_DIVIDE_BY_ZERO:
                case EXCEPTION_FLT_DIVIDE_BY_ZERO:
                case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
                    return EXCEPTION_EXECUTE_HANDLER;
                default:
                    return EXCEPTION_CONTINUE_SEARCH;
            }
        }

        // Returns 0 on success, the exception code on SEH fault.
        static unsigned long InvokeHandlerSEH(BML_ImcHandler handler,
                                               BML_Context ctx,
                                               BML_TopicId topic_id,
                                               BML_ImcMessage *imc_msg,
                                               void *user_data) {
            __try {
                handler(ctx, topic_id, imc_msg, user_data);
            } __except (FilterModuleException(GetExceptionCode())) {
                return GetExceptionCode();
            }
            return 0;
        }

        // Returns 0 on success, the exception code on SEH fault.
        // out_result receives the handler's return value on success.
        static unsigned long InvokeInterceptHandlerSEH(BML_ImcInterceptHandler handler,
                                                        BML_Context ctx,
                                                        BML_TopicId topic_id,
                                                        BML_ImcMessage *imc_msg,
                                                        void *user_data,
                                                        BML_EventResult *out_result) {
            __try {
                *out_result = handler(ctx, topic_id, imc_msg, user_data);
            } __except (FilterModuleException(GetExceptionCode())) {
                return GetExceptionCode();
            }
            return 0;
        }
#endif

        size_t ImcBusImpl::DrainSubscription(BML_Subscription_T *sub, size_t budget) {
            if (!sub || !sub->queue)
                return 0;

            size_t processed = 0;
            QueuedMessage *msg = nullptr;

            while ((budget == 0 || processed < budget) && sub->queue->Dequeue(msg)) {
                if (!sub->closed.load(std::memory_order_acquire) && sub->handler) {
                    BML_ImcMessage imc_msg = {};
                    imc_msg.struct_size = sizeof(BML_ImcMessage);
                    imc_msg.data = msg->Data();
                    imc_msg.size = msg->Size();
                    imc_msg.msg_id = msg->msg_id;
                    imc_msg.flags = msg->flags;
                    imc_msg.priority = msg->priority;
                    imc_msg.timestamp = msg->timestamp;
                    imc_msg.reply_topic = msg->reply_topic;
                    InvokeRegularHandler(sub, msg->topic_id, imc_msg);
                }
                ReleaseMessage(msg);
                ++processed;
            }
            return processed;
        }

        void ImcBusImpl::CleanupRetiredSubscriptions() {
            std::vector<std::unique_ptr<BML_Subscription_T>> ready;
            {
                std::lock_guard lock(m_WriteMutex);
                auto it = m_RetiredSubscriptions.begin();
                while (it != m_RetiredSubscriptions.end()) {
                    if ((*it)->ref_count.load(std::memory_order_acquire) == 0) {
                        ready.push_back(std::move(*it));
                        it = m_RetiredSubscriptions.erase(it);
                    } else {
                        ++it;
                    }
                }
            }

            for (auto &sub : ready) {
                DropPendingMessages(sub.get());
            }
        }

        void ImcBusImpl::RemoveFromMutableTopicMap(BML_TopicId topic, SubscriptionPtr handle) {
            auto it = m_MutableTopicMap.find(topic);
            if (it != m_MutableTopicMap.end()) {
                auto &vec = it->second;
                vec.erase(std::remove(vec.begin(), vec.end(), handle), vec.end());
                if (vec.empty())
                    m_MutableTopicMap.erase(it);
            }
        }

        void ImcBusImpl::PublishNewSnapshot() {
            // Build new immutable snapshot from current mutable state
            auto *snap = new TopicSnapshot();
            auto &registry = GetTopicRegistry();
            for (auto &[topic, handles] : m_MutableTopicMap) {
                auto &entry = snap->topic_subs[topic];
                entry.subs.reserve(handles.size());
                for (auto handle : handles) {
                    auto subIt = m_Subscriptions.find(handle);
                    if (subIt != m_Subscriptions.end()) {
                        auto *sub = subIt->second.get();
                        if (!sub->closed.load(std::memory_order_acquire)) {
                            entry.subs.push_back(sub);
                        }
                    }
                }
                entry.message_counter = registry.GetMessageCountPtr(topic);
            }
            // Build flat list for Pump
            snap->all_subs.reserve(m_Subscriptions.size());
            for (auto &[handle, sub] : m_Subscriptions) {
                if (!sub->closed.load(std::memory_order_acquire)) {
                    snap->all_subs.push_back(sub.get());
                }
            }

            // Atomically swap in the new snapshot
            TopicSnapshot *old = m_Snapshot.exchange(snap, std::memory_order_acq_rel);
            if (old) {
                m_RetiredSnapshots.push_back(old);
            }

            RetireOldSnapshots();
        }

        void ImcBusImpl::RetireOldSnapshots() {
            auto it = m_RetiredSnapshots.begin();
            while (it != m_RetiredSnapshots.end()) {
                if ((*it)->ref_count.load(std::memory_order_acquire) == 0) {
                    delete *it;
                    it = m_RetiredSnapshots.erase(it);
                } else {
                    ++it;
                }
            }
        }

        // ========================================================================
        // Pub/Sub API
        // ========================================================================

        BML_Result ImcBusImpl::Publish(BML_TopicId topic, const void *data, size_t size) {
            if (topic == BML_TOPIC_ID_INVALID)
                return BML_RESULT_INVALID_ARGUMENT;
            if (size > 0 && !data)
                return BML_RESULT_INVALID_ARGUMENT;

            auto *message = CreateMessage(topic, data, size, nullptr, nullptr);
            return DispatchMessage(topic, message);
        }

        BML_Result ImcBusImpl::PublishEx(BML_TopicId topic, const BML_ImcMessage *msg) {
            if (topic == BML_TOPIC_ID_INVALID)
                return BML_RESULT_INVALID_ARGUMENT;
            if (!msg)
                return BML_RESULT_INVALID_ARGUMENT;
            if (msg->size > 0 && !msg->data)
                return BML_RESULT_INVALID_ARGUMENT;

            auto *message = CreateMessage(topic, msg->data, msg->size, msg, nullptr);
            return DispatchMessage(topic, message);
        }

        BML_Result ImcBusImpl::PublishBuffer(BML_TopicId topic, const BML_ImcBuffer *buffer) {
            if (topic == BML_TOPIC_ID_INVALID || !buffer)
                return BML_RESULT_INVALID_ARGUMENT;
            if (buffer->size > 0 && !buffer->data)
                return BML_RESULT_INVALID_ARGUMENT;

            auto *message = CreateMessage(topic, nullptr, 0, nullptr, buffer);
            return DispatchMessage(topic, message);
        }

        BML_Result ImcBusImpl::Subscribe(BML_TopicId topic, BML_ImcHandler handler,
                                         void *user_data, BML_Subscription *out_sub) {
            BML_SubscribeOptions opts = BML_SUBSCRIBE_OPTIONS_INIT;
            return SubscribeEx(topic, handler, user_data, &opts, out_sub);
        }

        BML_Result ImcBusImpl::SubscribeEx(BML_TopicId topic, BML_ImcHandler handler,
                                           void *user_data, const BML_SubscribeOptions *options,
                                           BML_Subscription *out_sub) {
            if (topic == BML_TOPIC_ID_INVALID || !handler || !out_sub)
                return BML_RESULT_INVALID_ARGUMENT;

            BML_Mod owner = nullptr;
            BML_CHECK(ResolveRegistrationOwner(&owner));

            size_t capacity = kDefaultQueueCapacity;
            uint32_t min_priority = BML_IMC_PRIORITY_LOW;
            uint32_t backpressure = BML_BACKPRESSURE_DROP_OLDEST;
            uint32_t subscribe_flags = BML_IMC_SUBSCRIBE_FLAG_NONE;
            BML_ImcFilter filter = nullptr;
            void *filter_user_data = nullptr;

            if (options) {
                const size_t struct_size = options->struct_size;

                if (struct_size >= offsetof(BML_SubscribeOptions, queue_capacity) +
                        sizeof(BML_SubscribeOptions::queue_capacity)) {
                    capacity = options->queue_capacity > 0 ? options->queue_capacity : kDefaultQueueCapacity;
                    capacity = std::min(capacity, kMaxQueueCapacity);
                }
                if (struct_size >= offsetof(BML_SubscribeOptions, backpressure) +
                        sizeof(BML_SubscribeOptions::backpressure)) {
                    backpressure = options->backpressure;
                }
                if (struct_size >= offsetof(BML_SubscribeOptions, filter) +
                        sizeof(BML_SubscribeOptions::filter)) {
                    filter = options->filter;
                }
                if (struct_size >= offsetof(BML_SubscribeOptions, filter_user_data) +
                        sizeof(BML_SubscribeOptions::filter_user_data)) {
                    filter_user_data = options->filter_user_data;
                }
                if (struct_size >= offsetof(BML_SubscribeOptions, min_priority) +
                        sizeof(BML_SubscribeOptions::min_priority)) {
                    min_priority = options->min_priority;
                }
                if (struct_size >= offsetof(BML_SubscribeOptions, flags) +
                        sizeof(BML_SubscribeOptions::flags)) {
                    subscribe_flags = options->flags;
                }
            }

            auto subscription = std::make_unique<BML_Subscription_T>();
            subscription->topic_id = topic;
            subscription->handler = handler;
            subscription->user_data = user_data;
            subscription->owner = owner;
            // ref_count starts at 0 (default), tracks in-flight operations, not ownership
            subscription->queue_capacity = capacity;
            subscription->min_priority = min_priority;
            subscription->backpressure_policy = backpressure;
            subscription->filter = filter;
            subscription->filter_user_data = filter_user_data;
            subscription->InitStats();

            // Create priority queue
            subscription->queue = std::make_unique<PriorityMessageQueue<QueuedMessage *>>(capacity);
            if (!subscription->queue)
                return BML_RESULT_OUT_OF_MEMORY;

            BML_Subscription handle = subscription.get();
            {
                std::lock_guard lock(m_WriteMutex);
                m_MutableTopicMap[topic].push_back(handle);
                m_Subscriptions.emplace(handle, std::move(subscription));
                PublishNewSnapshot();
            }

            *out_sub = handle;

            if ((subscribe_flags & BML_IMC_SUBSCRIBE_FLAG_DELIVER_RETAINED_ON_SUBSCRIBE) != 0) {
                RetainedStateEntry retained;
                bool has_retained = false;
                {
                    std::lock_guard<std::mutex> state_lock(m_StateMutex);
                    auto state_it = m_RetainedStates.find(topic);
                    if (state_it != m_RetainedStates.end()) {
                        retained.owner = state_it->second.owner;
                        retained.timestamp = state_it->second.timestamp;
                        retained.flags = state_it->second.flags;
                        retained.priority = state_it->second.priority;
                        has_retained = retained.payload.CopyFrom(state_it->second.payload.Data(),
                                                                state_it->second.payload.Size());
                    }
                }

                if (has_retained) {
                    BML_ImcMessage retained_msg = {};
                    retained_msg.struct_size = sizeof(BML_ImcMessage);
                    retained_msg.data = retained.payload.Data();
                    retained_msg.size = retained.payload.Size();
                    retained_msg.flags = retained.flags;
                    retained_msg.priority = retained.priority;
                    retained_msg.timestamp = retained.timestamp;
                    if (MatchesSubscription(handle, retained_msg)) {
                        handle->RecordReceived(retained_msg.size);
                        m_Stats.total_messages_delivered.fetch_add(1, std::memory_order_relaxed);
                        handle->ref_count.fetch_add(1, std::memory_order_relaxed);
                        InvokeRegularHandler(handle, topic, retained_msg);
                        handle->ref_count.fetch_sub(1, std::memory_order_acq_rel);
                    }
                }
            }

            return BML_RESULT_OK;
        }

        BML_Result ImcBusImpl::SubscribeIntercept(BML_TopicId topic, BML_ImcInterceptHandler handler,
                                                  void *user_data, BML_Subscription *out_sub) {
            return SubscribeInterceptEx(topic, handler, user_data, nullptr, out_sub);
        }

        BML_Result ImcBusImpl::SubscribeInterceptEx(BML_TopicId topic, BML_ImcInterceptHandler handler,
                                                    void *user_data, const BML_SubscribeOptions *options,
                                                    BML_Subscription *out_sub) {
            if (topic == BML_TOPIC_ID_INVALID || !handler || !out_sub)
                return BML_RESULT_INVALID_ARGUMENT;

            BML_Mod owner = nullptr;
            BML_CHECK(ResolveRegistrationOwner(&owner));

            int32_t exec_order = 0;

            if (options && options->struct_size >=
                    offsetof(BML_SubscribeOptions, execution_order) + sizeof(int32_t)) {
                exec_order = options->execution_order;
            }

            auto subscription = std::make_unique<BML_Subscription_T>();
            subscription->topic_id = topic;
            subscription->intercept_handler = handler;
            subscription->user_data = user_data;
            subscription->owner = owner;
            subscription->queue_capacity = 0; // No queue for intercept subscriptions
            subscription->execution_order = exec_order;
            subscription->InitStats();
            // Intercept subscriptions are dispatched synchronously in
            // PublishInterceptable, not through the queue/drain path.
            // No queue needed.

            BML_Subscription handle = subscription.get();
            {
                std::lock_guard lock(m_WriteMutex);
                m_MutableTopicMap[topic].push_back(handle);
                m_Subscriptions.emplace(handle, std::move(subscription));
                PublishNewSnapshot();
            }

            *out_sub = handle;
            return BML_RESULT_OK;
        }

        BML_Result ImcBusImpl::PublishInterceptable(BML_TopicId topic, BML_ImcMessage *msg,
                                                    BML_EventResult *out_result) {
            if (topic == BML_TOPIC_ID_INVALID)
                return BML_RESULT_INVALID_ARGUMENT;
            if (!msg)
                return BML_RESULT_INVALID_ARGUMENT;

            BML_EventResult final_result = BML_EVENT_CONTINUE;
            BML_Context ctx = GetKernelOrNull()->context->GetHandle();

            // Phase 1: Run intercept handlers in execution_order
            // Collect interceptors under SnapshotGuard, then release guard before dispatch
            StackVec<BML_Subscription_T *, 8> interceptors;
            {
                SnapshotGuard guard(m_Snapshot.load(std::memory_order_acquire));
                if (guard) {
                    auto it = guard.get()->topic_subs.find(topic);
                    if (it != guard.get()->topic_subs.end()) {
                        const auto &subs = it->second.subs;
                        for (size_t i = 0; i < subs.size(); ++i) {
                            BML_Subscription_T *sub = subs[i];
                            if (sub && !sub->closed.load(std::memory_order_acquire) && sub->IsIntercept()) {
                                sub->ref_count.fetch_add(1, std::memory_order_relaxed);
                                interceptors.push_back(sub);
                            }
                        }
                    }
                }
            }

            if (!interceptors.empty()) {
                // stable_sort preserves subscription order among equal execution_order
                std::stable_sort(interceptors.begin(), interceptors.end(),
                                 [](const BML_Subscription_T *a, const BML_Subscription_T *b) {
                                     return a->execution_order < b->execution_order;
                                 });

                bool stopped = false;
                for (size_t idx = 0; idx < interceptors.size(); ++idx) {
                    BML_Subscription_T *sub = interceptors[idx];
                    if (!stopped && !sub->closed.load(std::memory_order_acquire) && sub->intercept_handler) {
                        BML_EventResult result = BML_EVENT_CONTINUE;
                        try {
                            DispatchSubscriptionScope dispatch_scope(sub);
                            if (dispatch_scope.Overflowed())
                                continue;
#if defined(_MSC_VER) && !defined(__MINGW32__)
                            unsigned long seh_code = InvokeInterceptHandlerSEH(
                                sub->intercept_handler, ctx, topic, msg, sub->user_data, &result);
                            if (seh_code != 0) {
                                std::string owner_id;
                                if (sub->owner) {
                                    auto *mod = GetKernelOrNull()->context->ResolveModHandle(sub->owner);
                                    if (mod) owner_id = mod->id;
                                }
                                CoreLog(BML_LOG_ERROR, kImcLogCategory,
                                        "Intercept handler crashed (code 0x%08lX) on topic %u, "
                                        "owned by module '%s' -- unsubscribing",
                                        seh_code, static_cast<unsigned>(topic),
                                        owner_id.empty() ? "unknown" : owner_id.c_str());
                                GetKernelOrNull()->crash_dump->WriteDumpOnce(owner_id, seh_code);
                                if (!owner_id.empty()) {
                                    GetKernelOrNull()->fault_tracker->RecordFault(owner_id, seh_code);
                                }
                                sub->closed.store(true, std::memory_order_release);
                                result = BML_EVENT_CONTINUE; // Treat crash as CONTINUE
                            } else {
                                sub->RecordProcessed();
                            }
#else
                            result = sub->intercept_handler(ctx, topic, msg, sub->user_data);
                            sub->RecordProcessed();
#endif
                        } catch (...) {
                            // Intercept handler threw -- treat as CONTINUE
                        }
                        if (result == BML_EVENT_HANDLED || result == BML_EVENT_CANCEL) {
                            final_result = result;
                            stopped = true;
                        }
                    }
                    sub->ref_count.fetch_sub(1, std::memory_order_acq_rel);
                }
            }

            // Phase 2: If not cancelled, deliver to regular (non-intercept) subscribers
            if (final_result != BML_EVENT_CANCEL) {
                BML_Result pub_result = PublishEx(topic, msg);
                if (pub_result != BML_RESULT_OK && pub_result != BML_RESULT_NOT_FOUND) {
                    if (out_result) *out_result = final_result;
                    return pub_result;
                }
            }

            if (out_result) *out_result = final_result;
            return BML_RESULT_OK;
        }

        BML_Result ImcBusImpl::GetSubscriptionStats(BML_Subscription sub, BML_SubscriptionStats *out_stats) {
            if (!sub)
                return BML_RESULT_INVALID_HANDLE;
            if (!out_stats)
                return BML_RESULT_INVALID_ARGUMENT;

            std::lock_guard lock(m_WriteMutex);
            auto it = m_Subscriptions.find(sub);
            if (it == m_Subscriptions.end())
                return BML_RESULT_INVALID_HANDLE;

            it->second->FillStats(out_stats);
            return BML_RESULT_OK;
        }

        BML_Result ImcBusImpl::PublishMulti(const BML_TopicId *topics, size_t topic_count,
                                            const void *data, size_t size, const BML_ImcMessage *msg,
                                            size_t *out_delivered) {
            if (!topics || topic_count == 0)
                return BML_RESULT_INVALID_ARGUMENT;
            if (size > 0 && !data)
                return BML_RESULT_INVALID_ARGUMENT;

            size_t delivered = 0;
            BML_Result first_error = BML_RESULT_OK;

            for (size_t i = 0; i < topic_count; ++i) {
                if (topics[i] == BML_TOPIC_ID_INVALID)
                    continue;

                auto *message = CreateMessage(topics[i], data, size, msg, nullptr);
                BML_Result res = DispatchMessage(topics[i], message);
                if (res == BML_RESULT_OK) {
                    ++delivered;
                } else if (first_error == BML_RESULT_OK) {
                    first_error = res;
                }
            }

            if (out_delivered)
                *out_delivered = delivered;

            return delivered > 0 ? BML_RESULT_OK : first_error;
        }

        BML_Result ImcBusImpl::Unsubscribe(BML_Subscription sub) {
            if (!sub)
                return BML_RESULT_INVALID_HANDLE;

            std::unique_ptr<BML_Subscription_T> owned;
            BML_Subscription_T *raw = nullptr;
            {
                std::lock_guard lock(m_WriteMutex);
                auto it = m_Subscriptions.find(sub);
                if (it == m_Subscriptions.end())
                    return BML_RESULT_INVALID_HANDLE;

                auto *s = it->second.get();
                s->closed.store(true, std::memory_order_release);
                RemoveFromMutableTopicMap(s->topic_id, sub);
                owned = std::move(it->second);
                raw = owned.get();
                m_Subscriptions.erase(it);
                PublishNewSnapshot();
            }

            if (raw) {
                if (std::find(g_DispatchSubscriptionStack.begin(),
                              g_DispatchSubscriptionStack.end(),
                              raw) != g_DispatchSubscriptionStack.end()) {
                    std::lock_guard lock(m_WriteMutex);
                    m_RetiredSubscriptions.push_back(std::move(owned));
                    return BML_RESULT_OK;
                }

                // Wait for any in-flight dispatch or pump work to finish before draining
                constexpr int kMaxSpinIterations = 100000;
                for (int i = 0; i < kMaxSpinIterations; ++i) {
                    if (raw->ref_count.load(std::memory_order_acquire) == 0)
                        break;
                    if (i < 1000)
                        std::this_thread::yield();
                    else
                        std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
                if (raw->ref_count.load(std::memory_order_acquire) != 0) {
                    CoreLog(BML_LOG_WARN, kImcLogCategory,
                            "Subscription ref_count non-zero after timeout, deferring cleanup");
                    std::lock_guard lock(m_WriteMutex);
                    m_RetiredSubscriptions.push_back(std::move(owned));
                    return BML_RESULT_OK;
                }
                DropPendingMessages(raw);
            }
            CleanupRetiredSubscriptions();
            return BML_RESULT_OK;
        }

        BML_Result ImcBusImpl::SubscriptionIsActive(BML_Subscription sub, BML_Bool *out_active) {
            if (!sub)
                return BML_RESULT_INVALID_HANDLE;
            if (!out_active)
                return BML_RESULT_INVALID_ARGUMENT;

            std::lock_guard lock(m_WriteMutex);
            auto it = m_Subscriptions.find(sub);
            if (it == m_Subscriptions.end()) {
                *out_active = BML_FALSE;
                return BML_RESULT_INVALID_HANDLE;
            }

            *out_active = it->second->closed.load(std::memory_order_acquire) ? BML_FALSE : BML_TRUE;
            return BML_RESULT_OK;
        }

        // ========================================================================
        // RPC API
        // ========================================================================

        BML_Result ImcBusImpl::RegisterRpc(BML_RpcId rpc_id, BML_RpcHandler handler, void *user_data) {
            if (rpc_id == BML_RPC_ID_INVALID || !handler)
                return BML_RESULT_INVALID_ARGUMENT;

            BML_Mod owner = nullptr;
            BML_CHECK(ResolveRegistrationOwner(&owner));

            std::unique_lock lock(m_RpcMutex);
            auto [it, inserted] = m_RpcHandlers.emplace(rpc_id, RpcHandlerEntry{});
            if (!inserted) {
                CoreLog(BML_LOG_WARN, kImcLogCategory,
                        "RPC handler already registered for ID 0x%08X", rpc_id);
                return BML_RESULT_ALREADY_EXISTS;
            }

            it->second.handler = handler;
            it->second.user_data = user_data;
            it->second.owner = owner;
            CoreLog(BML_LOG_DEBUG, kImcLogCategory,
                    "Registered RPC handler for ID 0x%08X", rpc_id);
            return BML_RESULT_OK;
        }

        BML_Result ImcBusImpl::UnregisterRpc(BML_RpcId rpc_id) {
            if (rpc_id == BML_RPC_ID_INVALID)
                return BML_RESULT_INVALID_ARGUMENT;

            BML_Mod owner = nullptr;
            BML_CHECK(ResolveRegistrationOwner(&owner));

            std::unique_lock lock(m_RpcMutex);
            // Check regular RPC handlers first, then streaming handlers
            auto it = m_RpcHandlers.find(rpc_id);
            if (it != m_RpcHandlers.end()) {
                if (it->second.owner != owner) {
                    return BML_RESULT_PERMISSION_DENIED;
                }
                m_RpcHandlers.erase(it);
                CoreLog(BML_LOG_DEBUG, kImcLogCategory,
                        "Unregistered RPC handler for ID 0x%08X", rpc_id);
                return BML_RESULT_OK;
            }
            auto sit = m_StreamingRpcHandlers.find(rpc_id);
            if (sit != m_StreamingRpcHandlers.end()) {
                if (sit->second.owner != owner) {
                    return BML_RESULT_PERMISSION_DENIED;
                }
                m_StreamingRpcHandlers.erase(sit);
                CoreLog(BML_LOG_DEBUG, kImcLogCategory,
                        "Unregistered streaming RPC handler for ID 0x%08X", rpc_id);
                return BML_RESULT_OK;
            }
            return BML_RESULT_NOT_FOUND;
        }

        BML_Result ImcBusImpl::CallRpc(BML_RpcId rpc_id, const BML_ImcMessage *request, BML_Future *out_future) {
            if (rpc_id == BML_RPC_ID_INVALID || !out_future)
                return BML_RESULT_INVALID_ARGUMENT;

            BML_Future future = CreateFuture();
            if (!future)
                return BML_RESULT_OUT_OF_MEMORY;

            auto *req = m_RpcRequestPool.Construct<RpcRequest>();
            if (!req) {
                FutureReleaseInternal(future);
                return BML_RESULT_OUT_OF_MEMORY;
            }

            req->rpc_id = rpc_id;
            req->msg_id = request && request->msg_id ? request->msg_id : m_NextMessageId.fetch_add(1, std::memory_order_relaxed);
            req->caller = Context::GetCurrentModule();
            req->future = future;

            if (request && request->data && request->size > 0) {
                if (!req->payload.CopyFrom(request->data, request->size)) {
                    m_RpcRequestPool.Destroy(req);
                    FutureReleaseInternal(future);
                    return BML_RESULT_OUT_OF_MEMORY;
                }
            }

            FutureAddRefInternal(future); // For the queue
            if (!m_RpcQueue->Enqueue(req)) {
                m_RpcRequestPool.Destroy(req);
                FutureReleaseInternal(future);
                FutureReleaseInternal(future);
                return BML_RESULT_WOULD_BLOCK;
            }

            m_Stats.total_rpc_calls.fetch_add(1, std::memory_order_relaxed);
            *out_future = future;
            return BML_RESULT_OK;
        }

        void ImcBusImpl::ProcessRpcRequest(RpcRequest *request) {
            if (!request)
                return;

            // Check deadline
            if (request->deadline_ns != 0 && GetTimestampNs() > request->deadline_ns) {
                if (request->future) {
                    request->future->CompleteWithError(BML_FUTURE_TIMEOUT, BML_RESULT_TIMEOUT, "RPC call timed out");
                    FutureReleaseInternal(request->future);
                }
                m_Stats.total_rpc_failures.fetch_add(1, std::memory_order_relaxed);
                m_RpcRequestPool.Destroy(request);
                return;
            }

            RpcHandlerEntry entry;
            std::shared_ptr<RpcHandlerStats> handler_stats;
            {
                std::shared_lock lock(m_RpcMutex);
                auto it = m_RpcHandlers.find(request->rpc_id);
                if (it == m_RpcHandlers.end()) {
                    if (request->future) {
                        request->future->Complete(BML_FUTURE_FAILED, BML_RESULT_NOT_FOUND, nullptr, 0);
                        FutureReleaseInternal(request->future);
                    }
                    m_Stats.total_rpc_failures.fetch_add(1, std::memory_order_relaxed);
                    m_RpcRequestPool.Destroy(request);
                    return;
                }
                entry = it->second;
                handler_stats = entry.stats;
            }

            BML_ImcBuffer response = BML_IMC_BUFFER_INIT;
            BML_ImcMessage req_msg = {};
            req_msg.struct_size = sizeof(BML_ImcMessage);
            req_msg.data = request->payload.Data();
            req_msg.size = request->payload.Size();
            req_msg.msg_id = request->msg_id;
            req_msg.flags = 0;

            BML_Context ctx = GetKernelOrNull()->context->GetHandle();

            // Snapshot middleware under lock
            std::vector<RpcMiddlewareEntry> middleware_snapshot;
            {
                std::shared_lock lock(m_RpcMutex);
                middleware_snapshot = m_RpcMiddleware;
            }

            // Run pre-middleware
            for (const auto &mw : middleware_snapshot) {
                BML_Result mw_result = BML_RESULT_OK;
                try {
                    mw_result = mw.middleware(ctx, request->rpc_id, BML_TRUE,
                                              &req_msg, nullptr, BML_RESULT_OK, mw.user_data);
                } catch (...) {
                    mw_result = BML_RESULT_INTERNAL_ERROR;
                }
                if (mw_result != BML_RESULT_OK) {
                    if (request->future) {
                        request->future->CompleteWithError(BML_FUTURE_FAILED, mw_result, "Pre-middleware rejected RPC");
                        FutureReleaseInternal(request->future);
                    }
                    m_Stats.total_rpc_failures.fetch_add(1, std::memory_order_relaxed);
                    m_RpcRequestPool.Destroy(request);
                    return;
                }
            }

            // Invoke handler
            auto start_time = GetTimestampNs();
            BML_Result result = BML_RESULT_INTERNAL_ERROR;
            char error_buf[512] = {};

            try {
                if (entry.handler_ex) {
                    result = entry.handler_ex(ctx, request->rpc_id, &req_msg, &response,
                                              error_buf, sizeof(error_buf), entry.user_data);
                } else if (entry.handler) {
                    result = entry.handler(ctx, request->rpc_id, &req_msg, &response, entry.user_data);
                }
            } catch (...) {
                result = BML_RESULT_INTERNAL_ERROR;
            }

            auto elapsed_ns = GetTimestampNs() - start_time;

            // Update per-handler stats (via shared_ptr, survives unregister)
            handler_stats->call_count.fetch_add(1, std::memory_order_relaxed);
            handler_stats->total_latency_ns.fetch_add(elapsed_ns, std::memory_order_relaxed);
            if (result == BML_RESULT_OK) {
                handler_stats->completion_count.fetch_add(1, std::memory_order_relaxed);
            } else {
                handler_stats->failure_count.fetch_add(1, std::memory_order_relaxed);
            }

            // Run post-middleware (informational, no short-circuit)
            for (const auto &mw : middleware_snapshot) {
                try {
                    mw.middleware(ctx, request->rpc_id, BML_FALSE,
                                 &req_msg, &response, result, mw.user_data);
                } catch (...) {
                    // Post-middleware errors are logged but don't affect result
                }
            }

            if (request->future) {
                if (result == BML_RESULT_OK) {
                    request->future->Complete(BML_FUTURE_READY, BML_RESULT_OK, response.data, response.size);
                    m_Stats.total_rpc_completions.fetch_add(1, std::memory_order_relaxed);
                } else {
                    request->future->CompleteWithError(BML_FUTURE_FAILED, result,
                                                       error_buf[0] ? error_buf : nullptr);
                    m_Stats.total_rpc_failures.fetch_add(1, std::memory_order_relaxed);
                }
                FutureReleaseInternal(request->future);
            }

            // Cleanup response if handler allocated it
            if (response.cleanup) {
                response.cleanup(response.data, response.size, response.cleanup_user_data);
            }

            m_RpcRequestPool.Destroy(request);
        }

        void ImcBusImpl::DrainRpcQueue(size_t budget) {
            size_t processed = 0;
            RpcRequest *req = nullptr;
            while ((budget == 0 || processed < budget) && m_RpcQueue->Dequeue(req)) {
                ProcessRpcRequest(req);
                ++processed;
            }
        }

        // ========================================================================
        // RPC v1.1 -- Extended Registration, Call, Introspection, Middleware, Streaming
        // ========================================================================

        BML_Result ImcBusImpl::RegisterRpcEx(BML_RpcId rpc_id, BML_RpcHandlerEx handler, void *user_data) {
            if (rpc_id == BML_RPC_ID_INVALID || !handler)
                return BML_RESULT_INVALID_ARGUMENT;

            BML_Mod owner = nullptr;
            BML_CHECK(ResolveRegistrationOwner(&owner));

            std::unique_lock lock(m_RpcMutex);
            auto [it, inserted] = m_RpcHandlers.emplace(rpc_id, RpcHandlerEntry{});
            if (!inserted) {
                return BML_RESULT_ALREADY_EXISTS;
            }
            it->second.handler_ex = handler;
            it->second.user_data = user_data;
            it->second.owner = owner;
            return BML_RESULT_OK;
        }

        BML_Result ImcBusImpl::CallRpcEx(BML_RpcId rpc_id, const BML_ImcMessage *request,
                                          const BML_RpcCallOptions *options, BML_Future *out_future) {
            if (rpc_id == BML_RPC_ID_INVALID || !out_future)
                return BML_RESULT_INVALID_ARGUMENT;

            BML_Future future = CreateFuture();
            if (!future)
                return BML_RESULT_OUT_OF_MEMORY;

            auto *req = m_RpcRequestPool.Construct<RpcRequest>();
            if (!req) {
                FutureReleaseInternal(future);
                return BML_RESULT_OUT_OF_MEMORY;
            }

            req->rpc_id = rpc_id;
            req->msg_id = request && request->msg_id ? request->msg_id : m_NextMessageId.fetch_add(1, std::memory_order_relaxed);
            req->caller = Context::GetCurrentModule();
            req->future = future;

            if (options && options->timeout_ms > 0) {
                req->deadline_ns = GetTimestampNs() +
                    static_cast<uint64_t>(options->timeout_ms) * 1000000ULL;
            }

            if (request && request->data && request->size > 0) {
                if (!req->payload.CopyFrom(request->data, request->size)) {
                    m_RpcRequestPool.Destroy(req);
                    FutureReleaseInternal(future);
                    return BML_RESULT_OUT_OF_MEMORY;
                }
            }

            FutureAddRefInternal(future);
            if (!m_RpcQueue->Enqueue(req)) {
                m_RpcRequestPool.Destroy(req);
                FutureReleaseInternal(future);
                FutureReleaseInternal(future);
                return BML_RESULT_WOULD_BLOCK;
            }

            m_Stats.total_rpc_calls.fetch_add(1, std::memory_order_relaxed);
            *out_future = future;
            return BML_RESULT_OK;
        }

        BML_Result ImcBusImpl::FutureGetError(BML_Future future, BML_Result *out_code,
                                               char *msg, size_t cap, size_t *out_len) {
            if (!future)
                return BML_RESULT_INVALID_HANDLE;

            std::lock_guard lock(future->mutex);
            if (out_code) *out_code = future->status;
            if (msg && cap > 0) {
                size_t len = future->error_message.size();
                size_t copy_len = (len < cap - 1) ? len : cap - 1;
                if (copy_len > 0)
                    std::memcpy(msg, future->error_message.c_str(), copy_len);
                msg[copy_len] = '\0';
                if (out_len) *out_len = len;
            } else if (out_len) {
                *out_len = future->error_message.size();
            }
            return BML_RESULT_OK;
        }

        BML_Result ImcBusImpl::GetRpcInfo(BML_RpcId rpc_id, BML_RpcInfo *out_info) {
            if (!out_info)
                return BML_RESULT_INVALID_ARGUMENT;

            std::memset(out_info, 0, sizeof(BML_RpcInfo));
            out_info->struct_size = sizeof(BML_RpcInfo);
            out_info->rpc_id = rpc_id;

            const std::string *name = GetRpcRegistry().GetName(rpc_id);
            if (name) {
                size_t copy_len = name->size() < 255 ? name->size() : 255;
                std::memcpy(out_info->name, name->c_str(), copy_len);
                out_info->name[copy_len] = '\0';
            }

            std::shared_lock lock(m_RpcMutex);
            auto it = m_RpcHandlers.find(rpc_id);
            if (it != m_RpcHandlers.end()) {
                out_info->has_handler = (it->second.handler || it->second.handler_ex) ? BML_TRUE : BML_FALSE;
                out_info->call_count = it->second.stats->call_count.load(std::memory_order_relaxed);
                out_info->completion_count = it->second.stats->completion_count.load(std::memory_order_relaxed);
                out_info->failure_count = it->second.stats->failure_count.load(std::memory_order_relaxed);
                out_info->total_latency_ns = it->second.stats->total_latency_ns.load(std::memory_order_relaxed);
            } else {
                out_info->has_handler = BML_FALSE;
            }
            return BML_RESULT_OK;
        }

        BML_Result ImcBusImpl::GetRpcName(BML_RpcId rpc_id, char *buf, size_t cap, size_t *out_len) {
            if (!buf || cap == 0)
                return BML_RESULT_INVALID_ARGUMENT;

            const std::string *name = GetRpcRegistry().GetName(rpc_id);
            if (!name) {
                buf[0] = '\0';
                if (out_len) *out_len = 0;
                return BML_RESULT_NOT_FOUND;
            }

            size_t len = name->size();
            if (len >= cap) {
                std::memcpy(buf, name->c_str(), cap - 1);
                buf[cap - 1] = '\0';
                if (out_len) *out_len = len;
                return BML_RESULT_BUFFER_TOO_SMALL;
            }

            std::memcpy(buf, name->c_str(), len);
            buf[len] = '\0';
            if (out_len) *out_len = len;
            return BML_RESULT_OK;
        }

        void ImcBusImpl::EnumerateRpc(void(*cb)(BML_RpcId, const char *, BML_Bool, void *), void *user_data) {
            if (!cb) return;

            // Snapshot handler IDs and handler presence under lock, then
            // invoke callback outside the lock to avoid deadlock if the
            // callback re-enters RPC registration.
            struct SnapshotEntry { BML_RpcId id; BML_Bool has_handler; };
            std::vector<SnapshotEntry> snapshot;
            {
                std::shared_lock lock(m_RpcMutex);
                snapshot.reserve(m_RpcHandlers.size() + m_StreamingRpcHandlers.size());
                for (const auto &[id, entry] : m_RpcHandlers) {
                    BML_Bool has = (entry.handler || entry.handler_ex) ? BML_TRUE : BML_FALSE;
                    snapshot.push_back({id, has});
                }
                for (const auto &[id, entry] : m_StreamingRpcHandlers) {
                    snapshot.push_back({id, entry.handler ? BML_TRUE : BML_FALSE});
                }
            }

            auto &registry = GetRpcRegistry();
            for (const auto &e : snapshot) {
                const std::string *name = registry.GetName(e.id);
                cb(e.id, name ? name->c_str() : "", e.has_handler, user_data);
            }
        }

        BML_Result ImcBusImpl::AddRpcMiddleware(BML_RpcMiddleware middleware, int32_t priority, void *user_data) {
            if (!middleware)
                return BML_RESULT_INVALID_ARGUMENT;

            BML_Mod owner = nullptr;
            BML_CHECK(ResolveRegistrationOwner(&owner));

            std::unique_lock lock(m_RpcMutex);
            RpcMiddlewareEntry mw;
            mw.middleware = middleware;
            mw.priority = priority;
            mw.user_data = user_data;
            mw.owner = owner;
            m_RpcMiddleware.push_back(mw);

            // Sort by priority (lower = first)
            std::sort(m_RpcMiddleware.begin(), m_RpcMiddleware.end(),
                      [](const RpcMiddlewareEntry &a, const RpcMiddlewareEntry &b) {
                          return a.priority < b.priority;
                      });
            return BML_RESULT_OK;
        }

        BML_Result ImcBusImpl::RemoveRpcMiddleware(BML_RpcMiddleware middleware) {
            if (!middleware)
                return BML_RESULT_INVALID_ARGUMENT;

            BML_Mod owner = nullptr;
            BML_CHECK(ResolveRegistrationOwner(&owner));

            std::unique_lock lock(m_RpcMutex);
            auto any_it = std::find_if(m_RpcMiddleware.begin(), m_RpcMiddleware.end(),
                                       [middleware](const RpcMiddlewareEntry &e) {
                                           return e.middleware == middleware;
                                       });
            if (any_it == m_RpcMiddleware.end()) {
                return BML_RESULT_NOT_FOUND;
            }
            if (any_it->owner != owner) {
                return BML_RESULT_PERMISSION_DENIED;
            }

            m_RpcMiddleware.erase(any_it);
            return BML_RESULT_OK;
        }

        BML_Result ImcBusImpl::RegisterStreamingRpc(BML_RpcId rpc_id, BML_StreamingRpcHandler handler, void *user_data) {
            if (rpc_id == BML_RPC_ID_INVALID || !handler)
                return BML_RESULT_INVALID_ARGUMENT;

            BML_Mod owner = nullptr;
            BML_CHECK(ResolveRegistrationOwner(&owner));

            std::unique_lock lock(m_RpcMutex);
            auto [it, inserted] = m_StreamingRpcHandlers.emplace(rpc_id, StreamingRpcHandlerEntry{});
            if (!inserted) {
                return BML_RESULT_ALREADY_EXISTS;
            }
            it->second.handler = handler;
            it->second.user_data = user_data;
            it->second.owner = owner;
            return BML_RESULT_OK;
        }

        BML_Result ImcBusImpl::StreamPush(BML_RpcStream stream, const void *data, size_t size) {
            if (!stream)
                return BML_RESULT_INVALID_HANDLE;

            auto *s = static_cast<BML_RpcStream_T *>(stream);
            if (s->completed.load(std::memory_order_acquire))
                return BML_RESULT_INVALID_STATE;

            if (s->on_chunk && s->chunk_topic != BML_TOPIC_ID_INVALID) {
                BML_ImcMessage msg = BML_IMC_MSG(data, size);
                BML_Context ctx = GetKernelOrNull()->context->GetHandle();
                s->on_chunk(ctx, s->chunk_topic, &msg, s->user_data);
            }
            return BML_RESULT_OK;
        }

        BML_Result ImcBusImpl::StreamComplete(BML_RpcStream stream) {
            if (!stream)
                return BML_RESULT_INVALID_HANDLE;

            auto *s = static_cast<BML_RpcStream_T *>(stream);
            bool expected = false;
            if (!s->completed.compare_exchange_strong(expected, true))
                return BML_RESULT_INVALID_STATE;

            if (s->future) {
                s->future->Complete(BML_FUTURE_READY, BML_RESULT_OK, nullptr, 0);
                FutureReleaseInternal(s->future);
                s->future = nullptr;
            }

            // Cleanup
            std::lock_guard lock(m_StreamMutex);
            m_Streams.erase(stream);
            return BML_RESULT_OK;
        }

        BML_Result ImcBusImpl::StreamError(BML_RpcStream stream, BML_Result error, const char *msg) {
            if (!stream)
                return BML_RESULT_INVALID_HANDLE;

            auto *s = static_cast<BML_RpcStream_T *>(stream);
            bool expected = false;
            if (!s->completed.compare_exchange_strong(expected, true))
                return BML_RESULT_INVALID_STATE;

            if (s->future) {
                s->future->CompleteWithError(BML_FUTURE_FAILED, error, msg);
                FutureReleaseInternal(s->future);
                s->future = nullptr;
            }

            std::lock_guard lock(m_StreamMutex);
            m_Streams.erase(stream);
            return BML_RESULT_OK;
        }

        BML_Result ImcBusImpl::CallStreamingRpc(BML_RpcId rpc_id, const BML_ImcMessage *request,
                                                 BML_ImcHandler on_chunk, BML_FutureCallback on_done,
                                                 void *user_data, BML_Future *out_future) {
            if (rpc_id == BML_RPC_ID_INVALID || !out_future)
                return BML_RESULT_INVALID_ARGUMENT;

            BML_Mod owner = nullptr;
            BML_CHECK(ResolveRegistrationOwner(&owner));

            StreamingRpcHandlerEntry handler_entry;
            {
                std::shared_lock lock(m_RpcMutex);
                auto it = m_StreamingRpcHandlers.find(rpc_id);
                if (it == m_StreamingRpcHandlers.end())
                    return BML_RESULT_NOT_FOUND;
                handler_entry = it->second;
            }

            BML_Future future = CreateFuture();
            if (!future)
                return BML_RESULT_OUT_OF_MEMORY;

            if (on_done) {
                future->callbacks.push_back({on_done, user_data});
            }

            // Create a dynamic topic for streaming chunks
            char topic_name[128];
            std::snprintf(topic_name, sizeof(topic_name), "BML/RpcStream/%u/%llu",
                          rpc_id, static_cast<unsigned long long>(m_NextMessageId.fetch_add(1, std::memory_order_relaxed)));
            BML_TopicId chunk_topic = BML_TOPIC_ID_INVALID;
            GetTopicId(topic_name, &chunk_topic);

            auto stream_state = std::make_unique<BML_RpcStream_T>();
            FutureAddRefInternal(future);
            stream_state->future = future;
            stream_state->chunk_topic = chunk_topic;
            stream_state->on_chunk = on_chunk;
            stream_state->on_done = on_done;
            stream_state->user_data = user_data;
            stream_state->owner = owner;

            BML_RpcStream stream = stream_state.get();
            {
                std::lock_guard lock(m_StreamMutex);
                m_Streams[stream] = std::move(stream_state);
            }

            // Invoke handler with the stream
            BML_ImcMessage req_msg = {};
            req_msg.struct_size = sizeof(BML_ImcMessage);
            if (request) {
                req_msg.data = request->data;
                req_msg.size = request->size;
                req_msg.msg_id = request->msg_id;
            }

            BML_Context ctx = GetKernelOrNull()->context->GetHandle();
            BML_Result result = BML_RESULT_INTERNAL_ERROR;
            try {
                result = handler_entry.handler(ctx, rpc_id, &req_msg, stream, handler_entry.user_data);
            } catch (...) {
                result = BML_RESULT_INTERNAL_ERROR;
            }

            if (result != BML_RESULT_OK) {
                // Handler failed to start streaming -- check if stream was
                // already completed/errored by the handler itself.
                std::lock_guard slock(m_StreamMutex);
                if (m_Streams.count(stream)) {
                    // Stream still alive -- error it out
                    auto *s = static_cast<BML_RpcStream_T *>(stream);
                    bool expected = false;
                    if (s->completed.compare_exchange_strong(expected, true)) {
                        future->CompleteWithError(BML_FUTURE_FAILED, result, "Streaming handler failed");
                    }
                    if (s->future) {
                        FutureReleaseInternal(s->future);
                        s->future = nullptr;
                    }
                    m_Streams.erase(stream);
                }
            }

            *out_future = future;
            return BML_RESULT_OK;
        }

        // ========================================================================
        // Future API
        // ========================================================================

        BML_Result ImcBusImpl::FutureAwait(BML_Future future, uint32_t timeout_ms) {
            if (!future)
                return BML_RESULT_INVALID_HANDLE;

            std::unique_lock lock(future->mutex);
            if (future->state != BML_FUTURE_PENDING)
                return BML_RESULT_OK;

            if (timeout_ms == 0) {
                future->cv.wait(lock, [future] { return future->state != BML_FUTURE_PENDING; });
            } else {
                auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
                if (!future->cv.wait_until(lock, deadline, [future] { return future->state != BML_FUTURE_PENDING; })) {
                    return BML_RESULT_TIMEOUT;
                }
            }
            return BML_RESULT_OK;
        }

        BML_Result ImcBusImpl::FutureGetResult(BML_Future future, BML_ImcMessage *out_msg) {
            if (!future)
                return BML_RESULT_INVALID_HANDLE;
            if (!out_msg)
                return BML_RESULT_INVALID_ARGUMENT;

            std::lock_guard lock(future->mutex);
            if (future->state == BML_FUTURE_PENDING)
                return BML_RESULT_NOT_FOUND;
            if (future->state != BML_FUTURE_READY)
                return future->status;

            out_msg->struct_size = sizeof(BML_ImcMessage);
            out_msg->data = future->payload.Data();
            out_msg->size = future->payload.Size();
            out_msg->msg_id = future->msg_id;
            out_msg->flags = future->flags;
            return BML_RESULT_OK;
        }

        BML_Result ImcBusImpl::FutureGetState(BML_Future future, BML_FutureState *out_state) {
            if (!future)
                return BML_RESULT_INVALID_HANDLE;
            if (!out_state)
                return BML_RESULT_INVALID_ARGUMENT;

            std::lock_guard lock(future->mutex);
            *out_state = future->state;
            return BML_RESULT_OK;
        }

        BML_Result ImcBusImpl::FutureCancel(BML_Future future) {
            if (!future)
                return BML_RESULT_INVALID_HANDLE;
            return future->Cancel() ? BML_RESULT_OK : BML_RESULT_INVALID_STATE;
        }

        BML_Result ImcBusImpl::FutureOnComplete(BML_Future future, BML_FutureCallback callback, void *user_data) {
            if (!future)
                return BML_RESULT_INVALID_HANDLE;
            if (!callback)
                return BML_RESULT_INVALID_ARGUMENT;

            bool invoke_now = false;
            {
                std::lock_guard lock(future->mutex);
                if (future->state == BML_FUTURE_PENDING) {
                    future->callbacks.push_back({callback, user_data});
                } else {
                    invoke_now = true;
                }
            }

            if (invoke_now) {
                BML_Context ctx = GetKernelOrNull()->context->GetHandle();
                callback(ctx, future, user_data);
            }
            return BML_RESULT_OK;
        }

        BML_Result ImcBusImpl::FutureRelease(BML_Future future) {
            if (!future)
                return BML_RESULT_INVALID_HANDLE;
            FutureReleaseInternal(future);
            return BML_RESULT_OK;
        }

        // ========================================================================
        // Pump & Shutdown
        // ========================================================================

        void ImcBusImpl::Pump(size_t max_per_sub) {
            // Update pump stats
            m_Stats.pump_cycles.fetch_add(1, std::memory_order_relaxed);
            m_Stats.last_pump_time.store(GetTimestampNs(), std::memory_order_relaxed);

            // Drain RPC queue
            DrainRpcQueue(max_per_sub);

            // Lock-free snapshot read for subscription draining (RCU pattern)
            SnapshotGuard guard(m_Snapshot.load(std::memory_order_acquire));
            if (!guard)
                return;

            for (auto *sub : guard.get()->all_subs) {
                if (sub->closed.load(std::memory_order_acquire))
                    continue;
                sub->ref_count.fetch_add(1, std::memory_order_relaxed);
                DrainSubscription(sub, max_per_sub);
                sub->ref_count.fetch_sub(1, std::memory_order_acq_rel);
            }

            CleanupRetiredSubscriptions();
        }

        void ImcBusImpl::Shutdown() {
            {
                std::lock_guard lock(m_WriteMutex);
                for (auto &[handle, sub] : m_Subscriptions) {
                    sub->closed.store(true, std::memory_order_release);
                }
                m_MutableTopicMap.clear();
                PublishNewSnapshot();

                for (auto &entry : m_Subscriptions) {
                    m_RetiredSubscriptions.push_back(std::move(entry.second));
                }
                m_Subscriptions.clear();
            }

            for (auto &sub : m_RetiredSubscriptions) {
                sub->closed.store(true, std::memory_order_release);
                while (sub->ref_count.load(std::memory_order_acquire) != 0) {
                    std::this_thread::yield();
                }
                DropPendingMessages(sub.get());
            }
            m_RetiredSubscriptions.clear();

            // Clean up all retired snapshots (spin until all readers are done)
            for (auto *snap : m_RetiredSnapshots) {
                while (snap->ref_count.load(std::memory_order_acquire) != 0) {
                    std::this_thread::yield();
                }
                delete snap;
            }
            m_RetiredSnapshots.clear();

            {
                std::lock_guard<std::mutex> state_lock(m_StateMutex);
                m_RetainedStates.clear();
            }

            // Clear in-flight streams
            {
                std::lock_guard stream_lock(m_StreamMutex);
                for (auto &[handle, state] : m_Streams) {
                    if (state && state->future) {
                        state->future->CompleteWithError(BML_FUTURE_CANCELLED, BML_RESULT_FAIL, "IMC shutdown");
                        FutureReleaseInternal(state->future);
                        state->future = nullptr;
                    }
                }
                m_Streams.clear();
            }

            RpcRequest *request = nullptr;
            while (m_RpcQueue && m_RpcQueue->Dequeue(request)) {
                if (!request) {
                    continue;
                }
                if (request->future) {
                    request->future->CompleteWithError(BML_FUTURE_CANCELLED, BML_RESULT_FAIL, "IMC shutdown");
                    FutureReleaseInternal(request->future);
                }
                m_RpcRequestPool.Destroy(request);
            }

            {
                std::unique_lock lock(m_RpcMutex);
                m_RpcHandlers.clear();
                m_StreamingRpcHandlers.clear();
                m_RpcMiddleware.clear();
            }
        }

        // ========================================================================
        // Statistics & Diagnostics
        // ========================================================================

        BML_Result ImcBusImpl::GetStats(BML_ImcStats *out_stats) {
            if (!out_stats)
                return BML_RESULT_INVALID_ARGUMENT;

            out_stats->struct_size = sizeof(BML_ImcStats);
            out_stats->total_messages_published = m_Stats.total_messages_published.load(std::memory_order_relaxed);
            out_stats->total_messages_delivered = m_Stats.total_messages_delivered.load(std::memory_order_relaxed);
            out_stats->total_messages_dropped = m_Stats.total_messages_dropped.load(std::memory_order_relaxed);
            out_stats->total_bytes_published = m_Stats.total_bytes_published.load(std::memory_order_relaxed);
            out_stats->total_rpc_calls = m_Stats.total_rpc_calls.load(std::memory_order_relaxed);
            out_stats->total_rpc_completions = m_Stats.total_rpc_completions.load(std::memory_order_relaxed);
            out_stats->total_rpc_failures = m_Stats.total_rpc_failures.load(std::memory_order_relaxed);

            // Count active subscriptions and topics
            {
                std::lock_guard lock(m_WriteMutex);
                out_stats->active_subscriptions = m_Subscriptions.size();
            }
            out_stats->active_topics = GetTopicRegistry().GetTopicCount();

            // Count active RPC handlers (regular + streaming)
            {
                std::shared_lock lock(m_RpcMutex);
                out_stats->active_rpc_handlers = m_RpcHandlers.size() + m_StreamingRpcHandlers.size();
            }

            out_stats->uptime_ns = GetTimestampNs() - m_Stats.start_time.load(std::memory_order_relaxed);
            return BML_RESULT_OK;
        }

        BML_Result ImcBusImpl::ResetStats() {
            m_Stats.Reset();
            return BML_RESULT_OK;
        }

        BML_Result ImcBusImpl::GetTopicInfo(BML_TopicId topic, BML_TopicInfo *out_info) {
            if (topic == BML_TOPIC_ID_INVALID || !out_info)
                return BML_RESULT_INVALID_ARGUMENT;

            out_info->struct_size = sizeof(BML_TopicInfo);
            out_info->topic_id = topic;
            out_info->message_count = GetTopicRegistry().GetMessageCount(topic);

            // Get subscription info from snapshot (lock-free read)
            {
                SnapshotGuard guard(m_Snapshot.load(std::memory_order_acquire));
                if (guard) {
                    auto it = guard.get()->topic_subs.find(topic);
                    if (it != guard.get()->topic_subs.end()) {
                        out_info->subscriber_count = it->second.subs.size();
                    } else {
                        out_info->subscriber_count = 0;
                    }
                } else {
                    out_info->subscriber_count = 0;
                }
            }

            // Get name if available
            const std::string *name = GetTopicRegistry().GetName(topic);
            if (name && !name->empty()) {
                size_t copy_len = std::min(name->size(), sizeof(out_info->name) - 1);
                std::memcpy(out_info->name, name->c_str(), copy_len);
                out_info->name[copy_len] = '\0';
            } else {
                out_info->name[0] = '\0';
            }

            return BML_RESULT_OK;
        }

        BML_Result ImcBusImpl::GetTopicName(BML_TopicId topic, char *buffer, size_t buffer_size, size_t *out_length) {
            if (topic == BML_TOPIC_ID_INVALID || !buffer || buffer_size == 0)
                return BML_RESULT_INVALID_ARGUMENT;

            const std::string *name = GetTopicRegistry().GetName(topic);
            if (!name) {
                buffer[0] = '\0';
                if (out_length) *out_length = 0;
                return BML_RESULT_NOT_FOUND;
            }

            size_t copy_len = std::min(name->size(), buffer_size - 1);
            std::memcpy(buffer, name->c_str(), copy_len);
            buffer[copy_len] = '\0';

            if (out_length)
                *out_length = name->size();

            return (name->size() < buffer_size) ? BML_RESULT_OK : BML_RESULT_BUFFER_TOO_SMALL;
        }

        BML_Result ImcBusImpl::PublishState(BML_TopicId topic, const BML_ImcMessage *msg) {
            if (topic == BML_TOPIC_ID_INVALID || !msg) {
                return BML_RESULT_INVALID_ARGUMENT;
            }
            if (msg->size > 0 && !msg->data) {
                return BML_RESULT_INVALID_ARGUMENT;
            }

            BML_Mod owner = nullptr;
            BML_CHECK(ResolveRegistrationOwner(&owner));

            RetainedStateEntry entry;
            entry.owner = owner;
            entry.timestamp = msg->timestamp ? msg->timestamp : GetTimestampNs();
            entry.flags = msg->flags;
            entry.priority = msg->priority;
            if (!entry.payload.CopyFrom(msg->data, msg->size)) {
                return BML_RESULT_OUT_OF_MEMORY;
            }

            {
                std::lock_guard<std::mutex> lock(m_StateMutex);
                m_RetainedStates[topic] = std::move(entry);
            }

            BML_ImcMessage publish_msg = *msg;
            publish_msg.timestamp = entry.timestamp;
            return PublishEx(topic, &publish_msg);
        }

        BML_Result ImcBusImpl::CopyState(BML_TopicId topic,
                                         void *dst,
                                         size_t dst_size,
                                         size_t *out_size,
                                         BML_ImcStateMeta *out_meta) {
            if (topic == BML_TOPIC_ID_INVALID) {
                return BML_RESULT_INVALID_ARGUMENT;
            }

            std::lock_guard<std::mutex> lock(m_StateMutex);
            auto it = m_RetainedStates.find(topic);
            if (it == m_RetainedStates.end()) {
                return BML_RESULT_NOT_FOUND;
            }

            const size_t payload_size = it->second.payload.Size();
            if (out_size) {
                *out_size = payload_size;
            }
            if (out_meta) {
                out_meta->struct_size = sizeof(BML_ImcStateMeta);
                out_meta->timestamp = it->second.timestamp;
                out_meta->flags = it->second.flags;
                out_meta->size = payload_size;
            }

            if (payload_size == 0) {
                return BML_RESULT_OK;
            }
            if (!dst && dst_size != 0) {
                return BML_RESULT_INVALID_ARGUMENT;
            }
            if (!dst || dst_size < payload_size) {
                return BML_RESULT_BUFFER_TOO_SMALL;
            }

            std::memcpy(dst, it->second.payload.Data(), payload_size);
            return BML_RESULT_OK;
        }

        BML_Result ImcBusImpl::ClearState(BML_TopicId topic) {
            if (topic == BML_TOPIC_ID_INVALID) {
                return BML_RESULT_INVALID_ARGUMENT;
            }

            std::lock_guard<std::mutex> lock(m_StateMutex);
            auto it = m_RetainedStates.find(topic);
            if (it == m_RetainedStates.end()) {
                return BML_RESULT_NOT_FOUND;
            }
            m_RetainedStates.erase(it);
            return BML_RESULT_OK;
        }

        void ImcBusImpl::CleanupOwner(BML_Mod owner) {
            if (!owner) {
                return;
            }

            std::vector<BML_Subscription> subscriptions;
            {
                std::lock_guard lock(m_WriteMutex);
                subscriptions.reserve(m_Subscriptions.size());
                for (const auto &[handle, sub] : m_Subscriptions) {
                    if (sub && sub->owner == owner) {
                        subscriptions.push_back(handle);
                    }
                }
            }

            for (auto sub : subscriptions) {
                Unsubscribe(sub);
            }

            {
                std::unique_lock lock(m_RpcMutex);
                for (auto it = m_RpcHandlers.begin(); it != m_RpcHandlers.end();) {
                    if (it->second.owner == owner) {
                        it = m_RpcHandlers.erase(it);
                    } else {
                        ++it;
                    }
                }
                for (auto it = m_StreamingRpcHandlers.begin(); it != m_StreamingRpcHandlers.end();) {
                    if (it->second.owner == owner) {
                        it = m_StreamingRpcHandlers.erase(it);
                    } else {
                        ++it;
                    }
                }
                for (auto it = m_RpcMiddleware.begin(); it != m_RpcMiddleware.end();) {
                    if (it->owner == owner) {
                        it = m_RpcMiddleware.erase(it);
                    } else {
                        ++it;
                    }
                }
            }

            {
                std::lock_guard<std::mutex> lock(m_StateMutex);
                for (auto it = m_RetainedStates.begin(); it != m_RetainedStates.end();) {
                    if (it->second.owner == owner) {
                        it = m_RetainedStates.erase(it);
                    } else {
                        ++it;
                    }
                }
            }

            {
                std::lock_guard lock(m_StreamMutex);
                for (auto it = m_Streams.begin(); it != m_Streams.end();) {
                    auto *state = it->second.get();
                    if (state && state->owner == owner) {
                        if (state->future) {
                            state->future->CompleteWithError(BML_FUTURE_CANCELLED,
                                                             BML_RESULT_FAIL,
                                                             "IMC owner cleanup");
                            FutureReleaseInternal(state->future);
                            state->future = nullptr;
                        }
                        it = m_Streams.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }
    } // namespace (anonymous)

    // ========================================================================
    // ImcBus wrapper owned by KernelServices around ImcBusImpl
    // ========================================================================

    namespace {
        void DeleteBusImpl(void *p) {
            delete static_cast<ImcBusImpl *>(p);
            g_BusPtr = nullptr;
        }
    }

    ImcBus::ImcBus()
        : m_Impl(new ImcBusImpl()),
          m_Deleter(&DeleteBusImpl) {
        g_BusPtr = static_cast<ImcBusImpl *>(m_Impl);
    }

    // ~ImcBus() is inline in ImcBus.h (calls m_Deleter).

    void ImcBus::Shutdown() {
        if (g_BusPtr)
            g_BusPtr->Shutdown();
    }

    // ========================================================================
    // Free functions wrapping ImcBusImpl
    // ========================================================================

    void ImcPump(size_t max_per_sub) { GetBus().Pump(max_per_sub); }
    void ImcShutdown() { GetBus().Shutdown(); }
    void ImcCleanupOwner(BML_Mod owner) { GetBus().CleanupOwner(owner); }

    BML_Result ImcGetTopicId(const char *n, BML_TopicId *o) { return GetBus().GetTopicId(n, o); }
    BML_Result ImcGetRpcId(const char *n, BML_RpcId *o) { return GetBus().GetRpcId(n, o); }
    BML_Result ImcPublish(BML_TopicId t, const void *d, size_t s) { return GetBus().Publish(t, d, s); }
    BML_Result ImcPublishEx(BML_TopicId t, const BML_ImcMessage *m) { return GetBus().PublishEx(t, m); }
    BML_Result ImcPublishBuffer(BML_TopicId t, const BML_ImcBuffer *b) { return GetBus().PublishBuffer(t, b); }
    BML_Result ImcSubscribe(BML_TopicId t, BML_ImcHandler h, void *u, BML_Subscription *o) { return GetBus().Subscribe(t, h, u, o); }
    BML_Result ImcSubscribeEx(BML_TopicId t, BML_ImcHandler h, void *u,
                              const BML_SubscribeOptions *opts, BML_Subscription *o) { return GetBus().SubscribeEx(t, h, u, opts, o); }
    BML_Result ImcUnsubscribe(BML_Subscription s) { return GetBus().Unsubscribe(s); }
    BML_Result ImcSubscriptionIsActive(BML_Subscription s, BML_Bool *o) { return GetBus().SubscriptionIsActive(s, o); }
    BML_Result ImcGetSubscriptionStats(BML_Subscription s, BML_SubscriptionStats *o) { return GetBus().GetSubscriptionStats(s, o); }
    BML_Result ImcSubscribeIntercept(BML_TopicId t, BML_ImcInterceptHandler h,
                                     void *u, BML_Subscription *o) { return GetBus().SubscribeIntercept(t, h, u, o); }
    BML_Result ImcSubscribeInterceptEx(BML_TopicId t, BML_ImcInterceptHandler h,
                                       void *u, const BML_SubscribeOptions *opts,
                                       BML_Subscription *o) { return GetBus().SubscribeInterceptEx(t, h, u, opts, o); }
    BML_Result ImcPublishInterceptable(BML_TopicId t, BML_ImcMessage *m, BML_EventResult *o) { return GetBus().PublishInterceptable(t, m, o); }
    BML_Result ImcRegisterRpc(BML_RpcId r, BML_RpcHandler h, void *u) { return GetBus().RegisterRpc(r, h, u); }
    BML_Result ImcUnregisterRpc(BML_RpcId r) { return GetBus().UnregisterRpc(r); }
    BML_Result ImcCallRpc(BML_RpcId r, const BML_ImcMessage *req, BML_Future *o) { return GetBus().CallRpc(r, req, o); }
    BML_Result ImcFutureAwait(BML_Future f, uint32_t t) { return GetBus().FutureAwait(f, t); }
    BML_Result ImcFutureGetResult(BML_Future f, BML_ImcMessage *o) { return GetBus().FutureGetResult(f, o); }
    BML_Result ImcFutureGetState(BML_Future f, BML_FutureState *o) { return GetBus().FutureGetState(f, o); }
    BML_Result ImcFutureCancel(BML_Future f) { return GetBus().FutureCancel(f); }
    BML_Result ImcFutureOnComplete(BML_Future f, BML_FutureCallback cb, void *u) { return GetBus().FutureOnComplete(f, cb, u); }
    BML_Result ImcFutureRelease(BML_Future f) { return GetBus().FutureRelease(f); }
    BML_Result ImcGetStats(BML_ImcStats *o) { return GetBus().GetStats(o); }
    BML_Result ImcResetStats() { return GetBus().ResetStats(); }
    BML_Result ImcGetTopicInfo(BML_TopicId t, BML_TopicInfo *o) { return GetBus().GetTopicInfo(t, o); }
    BML_Result ImcGetTopicName(BML_TopicId t, char *b, size_t s, size_t *o) { return GetBus().GetTopicName(t, b, s, o); }
    BML_Result ImcPublishState(BML_TopicId t, const BML_ImcMessage *m) { return GetBus().PublishState(t, m); }
    BML_Result ImcCopyState(BML_TopicId t, void *d, size_t ds, size_t *os, BML_ImcStateMeta *om) { return GetBus().CopyState(t, d, ds, os, om); }
    BML_Result ImcClearState(BML_TopicId t) { return GetBus().ClearState(t); }

    // RPC v1.1 free functions
    BML_Result ImcRegisterRpcEx(BML_RpcId r, BML_RpcHandlerEx h, void *u) { return GetBus().RegisterRpcEx(r, h, u); }
    BML_Result ImcCallRpcEx(BML_RpcId r, const BML_ImcMessage *req, const BML_RpcCallOptions *opts, BML_Future *o) { return GetBus().CallRpcEx(r, req, opts, o); }
    BML_Result ImcFutureGetError(BML_Future f, BML_Result *c, char *m, size_t cap, size_t *ol) { return GetBus().FutureGetError(f, c, m, cap, ol); }
    BML_Result ImcGetRpcInfo(BML_RpcId r, BML_RpcInfo *o) { return GetBus().GetRpcInfo(r, o); }
    BML_Result ImcGetRpcName(BML_RpcId r, char *b, size_t c, size_t *o) { return GetBus().GetRpcName(r, b, c, o); }
    void ImcEnumerateRpc(void(*cb)(BML_RpcId, const char *, BML_Bool, void *), void *u) { GetBus().EnumerateRpc(cb, u); }
    BML_Result ImcAddRpcMiddleware(BML_RpcMiddleware mw, int32_t p, void *u) { return GetBus().AddRpcMiddleware(mw, p, u); }
    BML_Result ImcRemoveRpcMiddleware(BML_RpcMiddleware mw) { return GetBus().RemoveRpcMiddleware(mw); }
    BML_Result ImcRegisterStreamingRpc(BML_RpcId r, BML_StreamingRpcHandler h, void *u) { return GetBus().RegisterStreamingRpc(r, h, u); }
    BML_Result ImcStreamPush(BML_RpcStream s, const void *d, size_t sz) { return GetBus().StreamPush(s, d, sz); }
    BML_Result ImcStreamComplete(BML_RpcStream s) { return GetBus().StreamComplete(s); }
    BML_Result ImcStreamError(BML_RpcStream s, BML_Result e, const char *m) { return GetBus().StreamError(s, e, m); }
    BML_Result ImcCallStreamingRpc(BML_RpcId r, const BML_ImcMessage *req, BML_ImcHandler oc, BML_FutureCallback od, void *u, BML_Future *o) { return GetBus().CallStreamingRpc(r, req, oc, od, u, o); }

    // ========================================================================
    // C API wrappers (registered with ApiRegistry)
    // ========================================================================

    namespace {
        BML_Result BML_API_ImcGetTopicId(const char *n, BML_TopicId *o) { return GetBus().GetTopicId(n, o); }
        BML_Result BML_API_ImcGetRpcId(const char *n, BML_RpcId *o) { return GetBus().GetRpcId(n, o); }
        BML_Result BML_API_ImcPublish(BML_TopicId t, const void *d, size_t s) { return GetBus().Publish(t, d, s); }
        BML_Result BML_API_ImcPublishEx(BML_TopicId t, const BML_ImcMessage *m) { return GetBus().PublishEx(t, m); }
        BML_Result BML_API_ImcPublishBuffer(BML_TopicId t, const BML_ImcBuffer *b) { return GetBus().PublishBuffer(t, b); }
        BML_Result BML_API_ImcPublishMulti(const BML_TopicId *ts, size_t n, const void *d, size_t s,
                                           const BML_ImcMessage *m, size_t *o) { return GetBus().PublishMulti(ts, n, d, s, m, o); }
        BML_Result BML_API_ImcSubscribe(BML_TopicId t, BML_ImcHandler h, void *u, BML_Subscription *o) { return GetBus().Subscribe(t, h, u, o); }
        BML_Result BML_API_ImcSubscribeEx(BML_TopicId t, BML_ImcHandler h, void *u,
                                          const BML_SubscribeOptions *opts, BML_Subscription *o) { return GetBus().SubscribeEx(t, h, u, opts, o); }
        BML_Result BML_API_ImcUnsubscribe(BML_Subscription s) { return GetBus().Unsubscribe(s); }
        BML_Result BML_API_ImcSubscriptionIsActive(BML_Subscription s, BML_Bool *o) { return GetBus().SubscriptionIsActive(s, o); }
        BML_Result BML_API_ImcSubscribeIntercept(BML_TopicId t, BML_ImcInterceptHandler h,
                                                  void *u, BML_Subscription *o) { return GetBus().SubscribeIntercept(t, h, u, o); }
        BML_Result BML_API_ImcSubscribeInterceptEx(BML_TopicId t, BML_ImcInterceptHandler h,
                                                    void *u, const BML_SubscribeOptions *opts,
                                                    BML_Subscription *o) { return GetBus().SubscribeInterceptEx(t, h, u, opts, o); }
        BML_Result BML_API_ImcPublishInterceptable(BML_TopicId t, BML_ImcMessage *m,
                                                    BML_EventResult *o) { return GetBus().PublishInterceptable(t, m, o); }
        BML_Result BML_API_ImcRegisterRpc(BML_RpcId r, BML_RpcHandler h, void *u) { return GetBus().RegisterRpc(r, h, u); }
        BML_Result BML_API_ImcUnregisterRpc(BML_RpcId r) { return GetBus().UnregisterRpc(r); }
        BML_Result BML_API_ImcCallRpc(BML_RpcId r, const BML_ImcMessage *req, BML_Future *o) { return GetBus().CallRpc(r, req, o); }
        BML_Result BML_API_ImcFutureAwait(BML_Future f, uint32_t t) { return GetBus().FutureAwait(f, t); }
        BML_Result BML_API_ImcFutureGetResult(BML_Future f, BML_ImcMessage *o) { return GetBus().FutureGetResult(f, o); }
        BML_Result BML_API_ImcFutureGetState(BML_Future f, BML_FutureState *o) { return GetBus().FutureGetState(f, o); }
        BML_Result BML_API_ImcFutureCancel(BML_Future f) { return GetBus().FutureCancel(f); }
        BML_Result BML_API_ImcFutureOnComplete(BML_Future f, BML_FutureCallback cb, void *u) { return GetBus().FutureOnComplete(f, cb, u); }
        BML_Result BML_API_ImcFutureRelease(BML_Future f) { return GetBus().FutureRelease(f); }
        void BML_API_ImcPump(size_t m) { GetBus().Pump(m); }
        BML_Result BML_API_ImcGetSubscriptionStats(BML_Subscription s, BML_SubscriptionStats *o) { return GetBus().GetSubscriptionStats(s, o); }
        BML_Result BML_API_ImcGetStats(BML_ImcStats *o) { return GetBus().GetStats(o); }
        BML_Result BML_API_ImcResetStats() { return GetBus().ResetStats(); }
        BML_Result BML_API_ImcGetTopicInfo(BML_TopicId t, BML_TopicInfo *o) { return GetBus().GetTopicInfo(t, o); }
        BML_Result BML_API_ImcGetTopicName(BML_TopicId t, char *b, size_t s, size_t *o) { return GetBus().GetTopicName(t, b, s, o); }
        BML_Result BML_API_ImcPublishState(BML_TopicId t, const BML_ImcMessage *m) { return GetBus().PublishState(t, m); }
        BML_Result BML_API_ImcCopyState(BML_TopicId t, void *d, size_t ds, size_t *os, BML_ImcStateMeta *om) { return GetBus().CopyState(t, d, ds, os, om); }
        BML_Result BML_API_ImcClearState(BML_TopicId t) { return GetBus().ClearState(t); }
        // RPC v1.1 C API wrappers
        BML_Result BML_API_ImcRegisterRpcEx(BML_RpcId r, BML_RpcHandlerEx h, void *u) { return GetBus().RegisterRpcEx(r, h, u); }
        BML_Result BML_API_ImcCallRpcEx(BML_RpcId r, const BML_ImcMessage *req, const BML_RpcCallOptions *opts, BML_Future *o) { return GetBus().CallRpcEx(r, req, opts, o); }
        BML_Result BML_API_ImcFutureGetError(BML_Future f, BML_Result *c, char *m, size_t cap, size_t *ol) { return GetBus().FutureGetError(f, c, m, cap, ol); }
        BML_Result BML_API_ImcGetRpcInfo(BML_RpcId r, BML_RpcInfo *o) { return GetBus().GetRpcInfo(r, o); }
        BML_Result BML_API_ImcGetRpcName(BML_RpcId r, char *b, size_t c, size_t *o) { return GetBus().GetRpcName(r, b, c, o); }
        void BML_API_ImcEnumerateRpc(void(*cb)(BML_RpcId, const char *, BML_Bool, void *), void *u) { GetBus().EnumerateRpc(cb, u); }
        BML_Result BML_API_ImcAddRpcMiddleware(BML_RpcMiddleware mw, int32_t p, void *u) { return GetBus().AddRpcMiddleware(mw, p, u); }
        BML_Result BML_API_ImcRemoveRpcMiddleware(BML_RpcMiddleware mw) { return GetBus().RemoveRpcMiddleware(mw); }
        BML_Result BML_API_ImcRegisterStreamingRpc(BML_RpcId r, BML_StreamingRpcHandler h, void *u) { return GetBus().RegisterStreamingRpc(r, h, u); }
        BML_Result BML_API_ImcStreamPush(BML_RpcStream s, const void *d, size_t sz) { return GetBus().StreamPush(s, d, sz); }
        BML_Result BML_API_ImcStreamComplete(BML_RpcStream s) { return GetBus().StreamComplete(s); }
        BML_Result BML_API_ImcStreamError(BML_RpcStream s, BML_Result e, const char *m) { return GetBus().StreamError(s, e, m); }
        BML_Result BML_API_ImcCallStreamingRpc(BML_RpcId r, const BML_ImcMessage *req, BML_ImcHandler oc, BML_FutureCallback od, void *u, BML_Future *o) { return GetBus().CallStreamingRpc(r, req, oc, od, u, o); }
    } // namespace

    void RegisterImcApis() {
        BML_BEGIN_API_REGISTRATION();

        BML_REGISTER_API_GUARDED(bmlImcGetTopicId, "imc", BML_API_ImcGetTopicId);
        BML_REGISTER_API_GUARDED(bmlImcGetRpcId, "imc", BML_API_ImcGetRpcId);
        BML_REGISTER_API_GUARDED(bmlImcPublish, "imc", BML_API_ImcPublish);
        BML_REGISTER_API_GUARDED(bmlImcPublishEx, "imc", BML_API_ImcPublishEx);
        BML_REGISTER_API_GUARDED(bmlImcPublishBuffer, "imc", BML_API_ImcPublishBuffer);
        BML_REGISTER_API_GUARDED(bmlImcPublishMulti, "imc", BML_API_ImcPublishMulti);
        BML_REGISTER_API_GUARDED(bmlImcSubscribe, "imc", BML_API_ImcSubscribe);
        BML_REGISTER_API_GUARDED(bmlImcSubscribeEx, "imc", BML_API_ImcSubscribeEx);
        BML_REGISTER_API_GUARDED(bmlImcUnsubscribe, "imc", BML_API_ImcUnsubscribe);
        BML_REGISTER_API_GUARDED(bmlImcSubscriptionIsActive, "imc", BML_API_ImcSubscriptionIsActive);
        BML_REGISTER_API_GUARDED(bmlImcSubscribeIntercept, "imc", BML_API_ImcSubscribeIntercept);
        BML_REGISTER_API_GUARDED(bmlImcSubscribeInterceptEx, "imc", BML_API_ImcSubscribeInterceptEx);
        BML_REGISTER_API_GUARDED(bmlImcPublishInterceptable, "imc", BML_API_ImcPublishInterceptable);
        BML_REGISTER_API_GUARDED(bmlImcRegisterRpc, "imc", BML_API_ImcRegisterRpc);
        BML_REGISTER_API_GUARDED(bmlImcUnregisterRpc, "imc", BML_API_ImcUnregisterRpc);
        BML_REGISTER_API_GUARDED(bmlImcCallRpc, "imc", BML_API_ImcCallRpc);
        BML_REGISTER_API_GUARDED(bmlImcFutureAwait, "imc", BML_API_ImcFutureAwait);
        BML_REGISTER_API_GUARDED(bmlImcFutureGetResult, "imc", BML_API_ImcFutureGetResult);
        BML_REGISTER_API_GUARDED(bmlImcFutureGetState, "imc", BML_API_ImcFutureGetState);
        BML_REGISTER_API_GUARDED(bmlImcFutureCancel, "imc", BML_API_ImcFutureCancel);
        BML_REGISTER_API_GUARDED(bmlImcFutureOnComplete, "imc", BML_API_ImcFutureOnComplete);
        BML_REGISTER_API_GUARDED(bmlImcFutureRelease, "imc", BML_API_ImcFutureRelease);
        BML_REGISTER_API_VOID_GUARDED(bmlImcPump, "imc", BML_API_ImcPump);
        BML_REGISTER_API_GUARDED(bmlImcGetSubscriptionStats, "imc", BML_API_ImcGetSubscriptionStats);
        BML_REGISTER_API_GUARDED(bmlImcGetStats, "imc", BML_API_ImcGetStats);
        BML_REGISTER_API_GUARDED(bmlImcResetStats, "imc", BML_API_ImcResetStats);
        BML_REGISTER_API_GUARDED(bmlImcGetTopicInfo, "imc", BML_API_ImcGetTopicInfo);
        BML_REGISTER_API_GUARDED(bmlImcGetTopicName, "imc", BML_API_ImcGetTopicName);
        BML_REGISTER_API_GUARDED(bmlImcPublishState, "imc", BML_API_ImcPublishState);
        BML_REGISTER_API_GUARDED(bmlImcCopyState, "imc", BML_API_ImcCopyState);
        BML_REGISTER_API_GUARDED(bmlImcClearState, "imc", BML_API_ImcClearState);
        // RPC v1.1
        BML_REGISTER_API_GUARDED(bmlImcRegisterRpcEx, "imc", BML_API_ImcRegisterRpcEx);
        BML_REGISTER_API_GUARDED(bmlImcCallRpcEx, "imc", BML_API_ImcCallRpcEx);
        BML_REGISTER_API_GUARDED(bmlImcFutureGetError, "imc", BML_API_ImcFutureGetError);
        BML_REGISTER_API_GUARDED(bmlImcGetRpcInfo, "imc", BML_API_ImcGetRpcInfo);
        BML_REGISTER_API_GUARDED(bmlImcGetRpcName, "imc", BML_API_ImcGetRpcName);
        BML_REGISTER_API_VOID_GUARDED(bmlImcEnumerateRpc, "imc", BML_API_ImcEnumerateRpc);
        BML_REGISTER_API_GUARDED(bmlImcAddRpcMiddleware, "imc", BML_API_ImcAddRpcMiddleware);
        BML_REGISTER_API_GUARDED(bmlImcRemoveRpcMiddleware, "imc", BML_API_ImcRemoveRpcMiddleware);
        BML_REGISTER_API_GUARDED(bmlImcRegisterStreamingRpc, "imc", BML_API_ImcRegisterStreamingRpc);
        BML_REGISTER_API_GUARDED(bmlImcStreamPush, "imc", BML_API_ImcStreamPush);
        BML_REGISTER_API_GUARDED(bmlImcStreamComplete, "imc", BML_API_ImcStreamComplete);
        BML_REGISTER_API_GUARDED(bmlImcStreamError, "imc", BML_API_ImcStreamError);
        BML_REGISTER_API_GUARDED(bmlImcCallStreamingRpc, "imc", BML_API_ImcCallStreamingRpc);
    }
} // namespace BML::Core


