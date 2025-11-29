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
#include "Context.h"
#include "FixedBlockPool.h"
#include "Logging.h"
#include "MpscRingBuffer.h"
#include "CoreErrors.h"

namespace {
    constexpr size_t kDefaultQueueCapacity = 256;
    constexpr size_t kMaxQueueCapacity = 16384;
    constexpr size_t kInlinePayloadBytes = 256;
    constexpr size_t kDefaultRpcQueueCapacity = 256;
    constexpr size_t kPriorityLevels = 4; // LOW, NORMAL, HIGH, URGENT

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

    // Lock-free priority queue with 4 priority levels and starvation mitigation.
    // Uses a weighted fair queuing approach: higher priority levels get more dequeue
    // opportunities but lower priorities are guaranteed some throughput.
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
    void *user_data{nullptr};
    BML_Mod owner{nullptr};
    std::atomic<uint32_t> ref_count{0};
    std::atomic<bool> closed{false};

    // Extended options
    size_t queue_capacity{kDefaultQueueCapacity};
    uint32_t min_priority{BML_IMC_PRIORITY_LOW};
    uint32_t backpressure_policy{BML_BACKPRESSURE_FAIL}; // Default to FAIL for explicit error handling

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
        BML_Context ctx = BML::Core::Context::Instance().GetHandle();
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

    struct RpcHandlerEntry {
        BML_RpcHandler handler{nullptr};
        void *user_data{nullptr};
        BML_Mod owner{nullptr};
    };

    struct RpcRequest {
        BML_RpcId rpc_id{0};
        BufferStorage payload;
        uint64_t msg_id{0};
        BML_Mod caller{nullptr};
        BML_Future future{nullptr};
    };
} // namespace (anonymous)

namespace BML::Core {
    // BML_Future_T and BML_Subscription_T are now in global scope - no import needed
    // Other types from anonymous namespace are accessible via unqualified lookup in same TU

    namespace {
        constexpr char kImcLogCategory[] = "imc.bus";

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
            BML_Result GetSubscriptionStats(BML_Subscription sub, BML_SubscriptionStats *out_stats);
            BML_Result PublishMulti(const BML_TopicId *topics, size_t topic_count,
                                    const void *data, size_t size, const BML_ImcMessage *msg,
                                    size_t *out_delivered);

            // RPC
            BML_Result RegisterRpc(BML_RpcId rpc_id, BML_RpcHandler handler, void *user_data);
            BML_Result UnregisterRpc(BML_RpcId rpc_id);
            BML_Result CallRpc(BML_RpcId rpc_id, const BML_ImcMessage *request, BML_Future *out_future);

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

            // Pump
            void Pump(size_t max_per_sub);
            void Shutdown();

        private:
            using SubscriptionPtr = BML_Subscription;

            QueuedMessage *CreateMessage(BML_TopicId topic, const void *data, size_t size,
                                         const BML_ImcMessage *msg, const BML_ImcBuffer *buffer);
            BML_Result DispatchMessage(BML_TopicId topic, QueuedMessage *message);
            BML_Result DispatchToSubscription(BML_Subscription_T *sub, QueuedMessage *message);
            void ReleaseMessage(QueuedMessage *message);
            void DropPendingMessages(BML_Subscription_T *sub);
            size_t DrainSubscription(BML_Subscription_T *sub, size_t budget);
            void RemoveFromTopicLocked(BML_TopicId topic, SubscriptionPtr handle);
            void ProcessRpcRequest(RpcRequest *request);
            void DrainRpcQueue(size_t budget);
            void ApplyBackpressure(BML_Subscription_T *sub, QueuedMessage *message);

            mutable std::shared_mutex m_Mutex;
            mutable std::shared_mutex m_RpcMutex;

            // Topic subscriptions (ID-based only)
            std::unordered_map<BML_TopicId, std::vector<SubscriptionPtr>> m_TopicMap;
            std::unordered_map<SubscriptionPtr, std::unique_ptr<BML_Subscription_T>> m_Subscriptions;

            // RPC handlers (ID-based only)
            std::unordered_map<BML_RpcId, RpcHandlerEntry> m_RpcHandlers;

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
        }

        ImcBusImpl &GetBus() {
            static ImcBusImpl bus;
            return bus;
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
            message->timestamp = GetTimestampNs();
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

        BML_Result ImcBusImpl::DispatchToSubscription(BML_Subscription_T *sub, QueuedMessage *message) {
            if (!sub || !message)
                return BML_RESULT_INVALID_ARGUMENT;

            // Check priority filter
            if (message->priority < sub->min_priority) {
                return BML_RESULT_OK; // Silently skip, not an error
            }

            // Try to enqueue
            // Note: ref_count is pre-allocated by DispatchMessage, no increment needed here
            if (sub->queue && sub->queue->Enqueue(message, message->priority)) {
                sub->RecordReceived(message->Size());
                m_Stats.total_messages_delivered.fetch_add(1, std::memory_order_relaxed);
                return BML_RESULT_OK;
            }

            // Queue full - apply backpressure policy based on configuration
            switch (sub->backpressure_policy) {
            case BML_BACKPRESSURE_DROP_OLDEST:
                ApplyBackpressure(sub, message);
                // Retry after dropping oldest
                if (sub->queue && sub->queue->Enqueue(message, message->priority)) {
                    sub->RecordReceived(message->Size());
                    m_Stats.total_messages_delivered.fetch_add(1, std::memory_order_relaxed);
                    return BML_RESULT_OK;
                }
                break;

            case BML_BACKPRESSURE_DROP_NEWEST:
                // Explicitly drop the new message - caller will release the ref
                sub->RecordDropped();
                m_Stats.total_messages_dropped.fetch_add(1, std::memory_order_relaxed);
                return BML_RESULT_OK; // Return OK since drop is intentional policy

            case BML_BACKPRESSURE_BLOCK:
                // Block policy: spin briefly then fail
                // In practice, a more sophisticated approach would use condition variables
                for (int spin = 0; spin < 100; ++spin) {
                    std::this_thread::yield();
                    if (sub->queue && sub->queue->Enqueue(message, message->priority)) {
                        sub->RecordReceived(message->Size());
                        m_Stats.total_messages_delivered.fetch_add(1, std::memory_order_relaxed);
                        return BML_RESULT_OK;
                    }
                }
                break;

            case BML_BACKPRESSURE_FAIL:
            default:
                // Fall through to failure path
                break;
            }

            // Failed to deliver
            sub->RecordDropped();
            m_Stats.total_messages_dropped.fetch_add(1, std::memory_order_relaxed);
            return BML_RESULT_WOULD_BLOCK;
        }

        BML_Result ImcBusImpl::DispatchMessage(BML_TopicId topic, QueuedMessage *message) {
            if (!message)
                return BML_RESULT_OUT_OF_MEMORY;

            // Register topic in registry for name lookup
            GetTopicRegistry().IncrementMessageCount(topic);

            std::vector<SubscriptionPtr> targets;
            {
                std::shared_lock lock(m_Mutex);
                auto it = m_TopicMap.find(topic);
                if (it == m_TopicMap.end()) {
                    ReleaseMessage(message);
                    return BML_RESULT_OK;
                }
                targets.reserve(it->second.size());

                // First pass: count valid targets and set message ref_count while holding lock
                // This ensures ref_count is set before any subscription can be released
                size_t valid_count = 0;
                for (auto handle : it->second) {
                    auto subIt = m_Subscriptions.find(handle);
                    if (subIt == m_Subscriptions.end())
                        continue;
                    auto *sub = subIt->second.get();
                    if (sub->closed.load(std::memory_order_acquire) || !sub->handler || !sub->queue)
                        continue;
                    ++valid_count;
                }

                if (valid_count == 0) {
                    ReleaseMessage(message);
                    return BML_RESULT_OK;
                }

                // Set ref_count BEFORE releasing lock to prevent race with subscription release
                // +1 for our own reference that we'll release at the end
                message->ref_count.store(valid_count + 1, std::memory_order_release);

                // Second pass: collect targets and increment their ref counts
                for (auto handle : it->second) {
                    auto subIt = m_Subscriptions.find(handle);
                    if (subIt == m_Subscriptions.end())
                        continue;
                    auto *sub = subIt->second.get();
                    if (sub->closed.load(std::memory_order_acquire) || !sub->handler || !sub->queue)
                        continue;
                    sub->ref_count.fetch_add(1, std::memory_order_relaxed);
                    targets.push_back(handle);
                }
            }

            // Note: message->ref_count is already set above, we just need to proceed with dispatch

            uint32_t delivered = 0;
            for (auto handle : targets) {
                auto *sub = reinterpret_cast<BML_Subscription_T *>(handle);
                if (DispatchToSubscription(sub, message) == BML_RESULT_OK) {
                    ++delivered;
                } else {
                    // Subscription didn't take ownership, release its ref
                    ReleaseMessage(message);
                }
                // Release subscription ref
                sub->ref_count.fetch_sub(1, std::memory_order_acq_rel);
            }

            ReleaseMessage(message); // Release our own reference
            return delivered > 0 ? BML_RESULT_OK : BML_RESULT_WOULD_BLOCK;
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

        size_t ImcBusImpl::DrainSubscription(BML_Subscription_T *sub, size_t budget) {
            if (!sub || !sub->queue)
                return 0;

            size_t processed = 0;
            QueuedMessage *msg = nullptr;
            BML_Context ctx = Context::Instance().GetHandle();

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

                    // Wrap handler in try-catch to ensure message is always released
                    try {
                        sub->handler(ctx, msg->topic_id, &imc_msg, sub->user_data);
                        sub->RecordProcessed();
                    } catch (...) {
                        // Handler threw exception - log and continue
                        // Message will still be released below
                    }
                }
                ReleaseMessage(msg);
                ++processed;
            }
            return processed;
        }

        void ImcBusImpl::RemoveFromTopicLocked(BML_TopicId topic, SubscriptionPtr handle) {
            auto it = m_TopicMap.find(topic);
            if (it != m_TopicMap.end()) {
                auto &vec = it->second;
                vec.erase(std::remove(vec.begin(), vec.end(), handle), vec.end());
                if (vec.empty())
                    m_TopicMap.erase(it);
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
            // Default options - use FAIL policy for explicit error handling
            BML_SubscribeOptions opts = {};
            opts.struct_size = sizeof(BML_SubscribeOptions);
            opts.queue_capacity = kDefaultQueueCapacity;
            opts.min_priority = BML_IMC_PRIORITY_LOW;
            opts.backpressure = BML_BACKPRESSURE_FAIL;
            opts.filter = nullptr;
            opts.filter_user_data = nullptr;
            return SubscribeEx(topic, handler, user_data, &opts, out_sub);
        }

        BML_Result ImcBusImpl::SubscribeEx(BML_TopicId topic, BML_ImcHandler handler,
                                           void *user_data, const BML_SubscribeOptions *options,
                                           BML_Subscription *out_sub) {
            if (topic == BML_TOPIC_ID_INVALID || !handler || !out_sub)
                return BML_RESULT_INVALID_ARGUMENT;

            // Parse options
            size_t capacity = kDefaultQueueCapacity;
            uint32_t min_priority = BML_IMC_PRIORITY_LOW;
            uint32_t backpressure = BML_BACKPRESSURE_DROP_OLDEST;

            if (options && options->struct_size >= sizeof(BML_SubscribeOptions)) {
                capacity = options->queue_capacity > 0 ? options->queue_capacity : kDefaultQueueCapacity;
                capacity = std::min(capacity, kMaxQueueCapacity);
                min_priority = options->min_priority;
                backpressure = options->backpressure;
            }

            auto subscription = std::make_unique<BML_Subscription_T>();
            subscription->topic_id = topic;
            subscription->handler = handler;
            subscription->user_data = user_data;
            subscription->owner = Context::GetCurrentModule();
            // ref_count starts at 0 (default), tracks in-flight operations, not ownership
            subscription->queue_capacity = capacity;
            subscription->min_priority = min_priority;
            subscription->backpressure_policy = backpressure;
            subscription->InitStats();

            // Create priority queue
            subscription->queue = std::make_unique<PriorityMessageQueue<QueuedMessage *>>(capacity);
            if (!subscription->queue)
                return BML_RESULT_OUT_OF_MEMORY;

            BML_Subscription handle = subscription.get();
            {
                std::unique_lock lock(m_Mutex);
                m_TopicMap[topic].push_back(handle);
                m_Subscriptions.emplace(handle, std::move(subscription));
            }

            *out_sub = handle;
            return BML_RESULT_OK;
        }

        BML_Result ImcBusImpl::GetSubscriptionStats(BML_Subscription sub, BML_SubscriptionStats *out_stats) {
            if (!sub)
                return BML_RESULT_INVALID_HANDLE;
            if (!out_stats)
                return BML_RESULT_INVALID_ARGUMENT;

            std::shared_lock lock(m_Mutex);
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
                std::unique_lock lock(m_Mutex);
                auto it = m_Subscriptions.find(sub);
                if (it == m_Subscriptions.end())
                    return BML_RESULT_INVALID_HANDLE;

                auto *s = it->second.get();
                s->closed.store(true, std::memory_order_release);
                RemoveFromTopicLocked(s->topic_id, sub);
                owned = std::move(it->second);
                raw = owned.get();
                m_Subscriptions.erase(it);
            }

            if (raw) {
                // Wait for any in-flight dispatch or pump work to finish before draining
                while (raw->ref_count.load(std::memory_order_acquire) != 0) {
                    std::this_thread::yield();
                }
                DropPendingMessages(raw);
            }
            return BML_RESULT_OK;
        }

        BML_Result ImcBusImpl::SubscriptionIsActive(BML_Subscription sub, BML_Bool *out_active) {
            if (!sub)
                return BML_RESULT_INVALID_HANDLE;
            if (!out_active)
                return BML_RESULT_INVALID_ARGUMENT;

            std::shared_lock lock(m_Mutex);
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

            std::unique_lock lock(m_RpcMutex);
            auto [it, inserted] = m_RpcHandlers.emplace(rpc_id, RpcHandlerEntry{});
            if (!inserted) {
                CoreLog(BML_LOG_WARN, kImcLogCategory,
                        "RPC handler already registered for ID 0x%08X", rpc_id);
                return BML_RESULT_ALREADY_EXISTS;
            }

            it->second.handler = handler;
            it->second.user_data = user_data;
            it->second.owner = Context::GetCurrentModule();
            CoreLog(BML_LOG_DEBUG, kImcLogCategory,
                    "Registered RPC handler for ID 0x%08X", rpc_id);
            return BML_RESULT_OK;
        }

        BML_Result ImcBusImpl::UnregisterRpc(BML_RpcId rpc_id) {
            if (rpc_id == BML_RPC_ID_INVALID)
                return BML_RESULT_INVALID_ARGUMENT;

            std::unique_lock lock(m_RpcMutex);
            auto it = m_RpcHandlers.find(rpc_id);
            if (it == m_RpcHandlers.end())
                return BML_RESULT_NOT_FOUND;

            m_RpcHandlers.erase(it);
            CoreLog(BML_LOG_DEBUG, kImcLogCategory,
                    "Unregistered RPC handler for ID 0x%08X", rpc_id);
            return BML_RESULT_OK;
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

            RpcHandlerEntry entry;
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
            }

            BML_ImcBuffer response = BML_IMC_BUFFER_INIT;
            BML_ImcMessage req_msg = {};
            req_msg.struct_size = sizeof(BML_ImcMessage);
            req_msg.data = request->payload.Data();
            req_msg.size = request->payload.Size();
            req_msg.msg_id = request->msg_id;
            req_msg.flags = 0;

            BML_Context ctx = Context::Instance().GetHandle();
            BML_Result result = BML_RESULT_INTERNAL_ERROR;

            // Wrap handler call in try-catch for exception safety
            try {
                result = entry.handler(ctx, request->rpc_id, &req_msg, &response, entry.user_data);
            } catch (...) {
                // Handler threw exception - treat as failure
                result = BML_RESULT_INTERNAL_ERROR;
            }

            if (request->future) {
                if (result == BML_RESULT_OK) {
                    request->future->Complete(BML_FUTURE_READY, BML_RESULT_OK, response.data, response.size);
                    m_Stats.total_rpc_completions.fetch_add(1, std::memory_order_relaxed);
                } else {
                    request->future->Complete(BML_FUTURE_FAILED, result, nullptr, 0);
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
                BML_Context ctx = Context::Instance().GetHandle();
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

            // Drain subscription queues
            std::vector<BML_Subscription_T *> subs;
            {
                std::shared_lock lock(m_Mutex);
                subs.reserve(m_Subscriptions.size());
                for (auto &[handle, sub] : m_Subscriptions) {
                    if (!sub->closed.load(std::memory_order_acquire)) {
                        sub->ref_count.fetch_add(1, std::memory_order_relaxed);
                        subs.push_back(sub.get());
                    }
                }
            }

            for (auto *sub : subs) {
                DrainSubscription(sub, max_per_sub);
                sub->ref_count.fetch_sub(1, std::memory_order_acq_rel);
            }
        }

        void ImcBusImpl::Shutdown() {
            std::unique_lock lock(m_Mutex);
            for (auto &[handle, sub] : m_Subscriptions) {
                sub->closed.store(true, std::memory_order_release);
                DropPendingMessages(sub.get());
            }
            m_Subscriptions.clear();
            m_TopicMap.clear();
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
                std::shared_lock lock(m_Mutex);
                out_stats->active_subscriptions = m_Subscriptions.size();
            }
            out_stats->active_topics = GetTopicRegistry().GetTopicCount();

            // Count active RPC handlers
            {
                std::shared_lock lock(m_RpcMutex);
                out_stats->active_rpc_handlers = m_RpcHandlers.size();
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

            // Get subscription info
            {
                std::shared_lock lock(m_Mutex);
                auto it = m_TopicMap.find(topic);
                if (it != m_TopicMap.end()) {
                    out_info->subscriber_count = it->second.size();
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
    } // namespace (anonymous)

    // ========================================================================
    // ImcBus Public Interface
    // ========================================================================

    ImcBus &ImcBus::Instance() {
        static ImcBus instance;
        return instance;
    }

    ImcBus::ImcBus() = default;

    BML_Result ImcBus::GetTopicId(const char *name, BML_TopicId *out_id) {
        return GetBus().GetTopicId(name, out_id);
    }

    BML_Result ImcBus::GetRpcId(const char *name, BML_RpcId *out_id) {
        return GetBus().GetRpcId(name, out_id);
    }

    BML_Result ImcBus::Publish(BML_TopicId topic, const void *data, size_t size) {
        return GetBus().Publish(topic, data, size);
    }

    BML_Result ImcBus::PublishEx(BML_TopicId topic, const BML_ImcMessage *msg) {
        return GetBus().PublishEx(topic, msg);
    }

    BML_Result ImcBus::PublishBuffer(BML_TopicId topic, const BML_ImcBuffer *buffer) {
        return GetBus().PublishBuffer(topic, buffer);
    }

    BML_Result ImcBus::Subscribe(BML_TopicId topic, BML_ImcHandler handler, void *user_data, BML_Subscription *out_sub) {
        return GetBus().Subscribe(topic, handler, user_data, out_sub);
    }

    BML_Result ImcBus::SubscribeEx(BML_TopicId topic, BML_ImcHandler handler, void *user_data,
                                   const BML_SubscribeOptions *options, BML_Subscription *out_sub) {
        return GetBus().SubscribeEx(topic, handler, user_data, options, out_sub);
    }

    BML_Result ImcBus::Unsubscribe(BML_Subscription sub) {
        return GetBus().Unsubscribe(sub);
    }

    BML_Result ImcBus::SubscriptionIsActive(BML_Subscription sub, BML_Bool *out_active) {
        return GetBus().SubscriptionIsActive(sub, out_active);
    }

    BML_Result ImcBus::GetSubscriptionStats(BML_Subscription sub, BML_SubscriptionStats *out_stats) {
        return GetBus().GetSubscriptionStats(sub, out_stats);
    }

    BML_Result ImcBus::PublishMulti(const BML_TopicId *topics, size_t topic_count,
                                    const void *data, size_t size, const BML_ImcMessage *msg,
                                    size_t *out_delivered) {
        return GetBus().PublishMulti(topics, topic_count, data, size, msg, out_delivered);
    }

    BML_Result ImcBus::RegisterRpc(BML_RpcId rpc_id, BML_RpcHandler handler, void *user_data) {
        return GetBus().RegisterRpc(rpc_id, handler, user_data);
    }

    BML_Result ImcBus::UnregisterRpc(BML_RpcId rpc_id) {
        return GetBus().UnregisterRpc(rpc_id);
    }

    BML_Result ImcBus::CallRpc(BML_RpcId rpc_id, const BML_ImcMessage *request, BML_Future *out_future) {
        return GetBus().CallRpc(rpc_id, request, out_future);
    }

    BML_Result ImcBus::FutureAwait(BML_Future future, uint32_t timeout_ms) {
        return GetBus().FutureAwait(future, timeout_ms);
    }

    BML_Result ImcBus::FutureGetResult(BML_Future future, BML_ImcMessage *out_msg) {
        return GetBus().FutureGetResult(future, out_msg);
    }

    BML_Result ImcBus::FutureGetState(BML_Future future, BML_FutureState *out_state) {
        return GetBus().FutureGetState(future, out_state);
    }

    BML_Result ImcBus::FutureCancel(BML_Future future) {
        return GetBus().FutureCancel(future);
    }

    BML_Result ImcBus::FutureOnComplete(BML_Future future, BML_FutureCallback callback, void *user_data) {
        return GetBus().FutureOnComplete(future, callback, user_data);
    }

    BML_Result ImcBus::FutureRelease(BML_Future future) {
        return GetBus().FutureRelease(future);
    }

    BML_Result ImcBus::GetStats(BML_ImcStats *out_stats) {
        return GetBus().GetStats(out_stats);
    }

    BML_Result ImcBus::ResetStats() {
        return GetBus().ResetStats();
    }

    BML_Result ImcBus::GetTopicInfo(BML_TopicId topic, BML_TopicInfo *out_info) {
        return GetBus().GetTopicInfo(topic, out_info);
    }

    BML_Result ImcBus::GetTopicName(BML_TopicId topic, char *buffer, size_t buffer_size, size_t *out_length) {
        return GetBus().GetTopicName(topic, buffer, buffer_size, out_length);
    }

    void ImcBus::Pump(size_t max_per_sub) {
        GetBus().Pump(max_per_sub);
    }

    void ImcBus::Shutdown() {
        GetBus().Shutdown();
    }
} // namespace BML::Core
