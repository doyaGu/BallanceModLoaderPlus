#ifndef BML_CORE_IMC_BUS_INTERNAL_H
#define BML_CORE_IMC_BUS_INTERNAL_H

#include "ImcBus.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Context.h"
#include "CoreErrors.h"
#include "CrashDumpWriter.h"
#include "FaultTracker.h"
#include "FixedBlockPool.h"
#include "Logging.h"
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

    inline uint64_t GetTimestampNs() noexcept {
        using namespace std::chrono;
        return static_cast<uint64_t>(
            duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
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
    extern Context *g_BusContext;
    extern ImcBusImpl *g_BusPtr;
    ImcBusImpl &GetBus();
} // namespace BML::Core

struct BML_Subscription_T {
    BML_TopicId topic_id{0};
    BML_ImcHandler handler{nullptr};
    BML_ImcInterceptHandler intercept_handler{nullptr};
    void *user_data{nullptr};
    BML_Mod owner{nullptr};
    std::atomic<uint32_t> ref_count{0};
    std::atomic<bool> closed{false};

    size_t queue_capacity{BML::Core::kDefaultQueueCapacity};
    uint32_t min_priority{BML_IMC_PRIORITY_LOW};
    uint32_t backpressure_policy{BML_BACKPRESSURE_FAIL};
    int32_t execution_order{0};
    BML_ImcFilter filter{nullptr};
    void *filter_user_data{nullptr};

    bool IsIntercept() const { return intercept_handler != nullptr; }

    std::unique_ptr<BML::Core::PriorityMessageQueue<BML::Core::QueuedMessage *>> queue;

    std::atomic<uint64_t> messages_received{0};
    std::atomic<uint64_t> messages_processed{0};
    std::atomic<uint64_t> messages_dropped{0};
    std::atomic<uint64_t> total_bytes_received{0};
    std::atomic<uint64_t> last_message_time{0};
    std::atomic<uint64_t> creation_time{0};

    void InitStats() {
        creation_time.store(BML::Core::GetTimestampNs(), std::memory_order_relaxed);
    }

    void RecordReceived(size_t bytes) {
        messages_received.fetch_add(1, std::memory_order_relaxed);
        total_bytes_received.fetch_add(bytes, std::memory_order_relaxed);
        last_message_time.store(BML::Core::GetTimestampNs(), std::memory_order_relaxed);
    }

    void RecordProcessed() {
        messages_processed.fetch_add(1, std::memory_order_relaxed);
    }

    void RecordDropped() {
        messages_dropped.fetch_add(1, std::memory_order_relaxed);
    }

    void FillStats(BML_SubscriptionStats *stats) const {
        if (!stats) {
            return;
        }
        stats->struct_size = sizeof(BML_SubscriptionStats);
        stats->messages_received = messages_received.load(std::memory_order_relaxed);
        stats->messages_processed = messages_processed.load(std::memory_order_relaxed);
        stats->messages_dropped = messages_dropped.load(std::memory_order_relaxed);
        stats->total_bytes = total_bytes_received.load(std::memory_order_relaxed);
        stats->queue_size = queue ? queue->ApproximateSize() : 0;
        stats->queue_capacity = queue_capacity * BML::Core::kPriorityLevels;
        stats->last_message_time = last_message_time.load(std::memory_order_relaxed);
    }
};

inline thread_local std::vector<BML_Subscription_T *> g_DispatchSubscriptionStack;

class DispatchSubscriptionScope {
public:
    explicit DispatchSubscriptionScope(BML_Subscription_T *sub) {
        constexpr size_t kMaxDispatchDepth = 64;
        if (g_DispatchSubscriptionStack.size() >= kMaxDispatchDepth) {
            BML::Core::CoreLog(BML_LOG_ERROR, BML::Core::kImcLogCategory,
                               "IMC dispatch stack overflow (depth %zu)",
                               g_DispatchSubscriptionStack.size());
            m_Overflow = true;
            return;
        }
        g_DispatchSubscriptionStack.push_back(sub);
    }

    ~DispatchSubscriptionScope() {
        if (!m_Overflow) {
            g_DispatchSubscriptionStack.pop_back();
        }
    }

    bool Overflowed() const { return m_Overflow; }

    DispatchSubscriptionScope(const DispatchSubscriptionScope &) = delete;
    DispatchSubscriptionScope &operator=(const DispatchSubscriptionScope &) = delete;

private:
    bool m_Overflow = false;
};

class ModuleContextScope {
public:
    explicit ModuleContextScope(BML_Mod) {}
    ~ModuleContextScope() = default;

    ModuleContextScope(const ModuleContextScope &) = delete;
    ModuleContextScope &operator=(const ModuleContextScope &) = delete;
};

struct FutureCallbackEntry {
    BML_FutureCallback fn{nullptr};
    void *user_data{nullptr};
    BML_Mod owner{nullptr};
};

struct BML_Future_T {
    std::atomic<uint32_t> ref_count{1};
    std::mutex mutex;
    std::condition_variable cv;
    BML_FutureState state{BML_FUTURE_PENDING};
    BML_Result status{BML_RESULT_OK};
    BML::Core::BufferStorage payload;
    uint64_t msg_id{0};
    uint32_t flags{0};
    uint64_t creation_time{0};
    uint64_t completion_time{0};
    std::string error_message;
    std::vector<FutureCallbackEntry> callbacks;

    BML_Future_T() : creation_time(BML::Core::GetTimestampNs()) {}

    void Complete(BML_FutureState new_state,
                  BML_Result new_status,
                  const void *data,
                  size_t size) {
        std::vector<FutureCallbackEntry> pending_callbacks;
        {
            std::unique_lock lock(mutex);
            if (state != BML_FUTURE_PENDING) {
                return;
            }
            state = new_state;
            status = new_status;
            completion_time = BML::Core::GetTimestampNs();
            if (new_state == BML_FUTURE_READY && new_status == BML_RESULT_OK &&
                data && size > 0) {
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
            if (state != BML_FUTURE_PENDING) {
                return;
            }
            state = new_state;
            status = new_status;
            completion_time = BML::Core::GetTimestampNs();
            if (err_msg && *err_msg) {
                error_message = err_msg;
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
            if (state != BML_FUTURE_PENDING) {
                return false;
            }
            state = BML_FUTURE_CANCELLED;
            status = BML_RESULT_FAIL;
            completion_time = BML::Core::GetTimestampNs();
            pending_callbacks = callbacks;
            callbacks.clear();
        }
        NotifyCallbacks(std::move(pending_callbacks));
        return true;
    }

    void NotifyCallbacks(std::vector<FutureCallbackEntry> &&pending_callbacks) {
        cv.notify_all();
        BML_Context ctx = BML::Core::g_BusContext->GetHandle();
        for (const auto &entry : pending_callbacks) {
            if (entry.fn) {
                ModuleContextScope scope(entry.owner);
                entry.fn(ctx, this, entry.user_data);
            }
        }
    }

    uint64_t GetLatencyNs() const {
        if (completion_time == 0) {
            return 0;
        }
        return completion_time - creation_time;
    }
};

struct BML_RpcStream_T {
    BML_Future future{nullptr};
    BML_TopicId chunk_topic{BML_TOPIC_ID_INVALID};
    BML_ImcHandler on_chunk{nullptr};
    BML_FutureCallback on_done{nullptr};
    void *user_data{nullptr};
    BML_Mod owner{nullptr};
    std::atomic<bool> completed{false};
};

inline BML_Future CreateFuture() {
    return new (std::nothrow) BML_Future_T();
}

inline void FutureAddRefInternal(BML_Future future) {
    if (future) {
        future->ref_count.fetch_add(1, std::memory_order_relaxed);
    }
}

inline void FutureReleaseInternal(BML_Future future) {
    if (!future) {
        return;
    }
    uint32_t previous = future->ref_count.fetch_sub(1, std::memory_order_acq_rel);
    if (previous == 1) {
        delete future;
    }
}

namespace BML::Core {
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
        uint64_t deadline_ns{0};
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

    struct TopicEntry {
        std::vector<BML_Subscription_T *> subs;
        std::atomic<uint64_t> *message_counter = nullptr;
    };

    struct TopicSnapshot {
        std::unordered_map<BML_TopicId, TopicEntry> topic_subs;
        std::vector<BML_Subscription_T *> all_subs;
        std::atomic<uint32_t> ref_count{0};

        TopicSnapshot() = default;
        TopicSnapshot(const TopicSnapshot &) = delete;
        TopicSnapshot &operator=(const TopicSnapshot &) = delete;
    };

    class SnapshotGuard {
    public:
        explicit SnapshotGuard(TopicSnapshot *snap) noexcept : m_Snap(snap) {
            if (m_Snap) {
                m_Snap->ref_count.fetch_add(1, std::memory_order_acq_rel);
            }
        }

        ~SnapshotGuard() {
            if (m_Snap) {
                m_Snap->ref_count.fetch_sub(1, std::memory_order_acq_rel);
            }
        }

        SnapshotGuard(const SnapshotGuard &) = delete;
        SnapshotGuard &operator=(const SnapshotGuard &) = delete;

        TopicSnapshot *get() const noexcept { return m_Snap; }
        explicit operator bool() const noexcept { return m_Snap != nullptr; }

    private:
        TopicSnapshot *m_Snap;
    };

    class ImcBusImpl {
    public:
        ImcBusImpl();

        BML_Result GetTopicId(const char *name, BML_TopicId *out_id);
        BML_Result GetRpcId(const char *name, BML_RpcId *out_id);

        BML_Result Publish(BML_TopicId topic, const void *data, size_t size);
        BML_Result Publish(BML_Mod owner, BML_TopicId topic, const void *data, size_t size);
        BML_Result PublishEx(BML_TopicId topic, const BML_ImcMessage *msg);
        BML_Result PublishEx(BML_Mod owner, BML_TopicId topic, const BML_ImcMessage *msg);
        BML_Result PublishBuffer(BML_TopicId topic, const BML_ImcBuffer *buffer);
        BML_Result PublishBuffer(BML_Mod owner, BML_TopicId topic,
                                 const BML_ImcBuffer *buffer);

        BML_Result Subscribe(BML_TopicId topic, BML_ImcHandler handler, void *user_data,
                             BML_Subscription *out_sub);
        BML_Result Subscribe(BML_Mod owner, BML_TopicId topic, BML_ImcHandler handler,
                             void *user_data, BML_Subscription *out_sub);
        BML_Result SubscribeEx(BML_TopicId topic, BML_ImcHandler handler, void *user_data,
                               const BML_SubscribeOptions *options,
                               BML_Subscription *out_sub);
        BML_Result SubscribeEx(BML_Mod owner, BML_TopicId topic, BML_ImcHandler handler,
                               void *user_data, const BML_SubscribeOptions *options,
                               BML_Subscription *out_sub);
        BML_Result Unsubscribe(BML_Subscription sub);
        BML_Result SubscriptionIsActive(BML_Subscription sub, BML_Bool *out_active);
        BML_Result GetSubscriptionStats(BML_Subscription sub,
                                        BML_SubscriptionStats *out_stats);

        BML_Result SubscribeIntercept(BML_TopicId topic, BML_ImcInterceptHandler handler,
                                      void *user_data, BML_Subscription *out_sub);
        BML_Result SubscribeIntercept(BML_Mod owner, BML_TopicId topic,
                                      BML_ImcInterceptHandler handler, void *user_data,
                                      BML_Subscription *out_sub);
        BML_Result SubscribeInterceptEx(BML_TopicId topic,
                                        BML_ImcInterceptHandler handler, void *user_data,
                                        const BML_SubscribeOptions *options,
                                        BML_Subscription *out_sub);
        BML_Result SubscribeInterceptEx(BML_Mod owner, BML_TopicId topic,
                                        BML_ImcInterceptHandler handler, void *user_data,
                                        const BML_SubscribeOptions *options,
                                        BML_Subscription *out_sub);
        BML_Result PublishInterceptable(BML_TopicId topic, BML_ImcMessage *msg,
                                        BML_EventResult *out_result);
        BML_Result PublishInterceptable(BML_Mod owner, BML_TopicId topic,
                                        BML_ImcMessage *msg, BML_EventResult *out_result);
        BML_Result PublishMulti(const BML_TopicId *topics, size_t topic_count,
                                const void *data, size_t size, const BML_ImcMessage *msg,
                                size_t *out_delivered);
        BML_Result PublishMulti(BML_Mod owner, const BML_TopicId *topics,
                                size_t topic_count, const void *data, size_t size,
                                const BML_ImcMessage *msg, size_t *out_delivered);

        BML_Result RegisterRpc(BML_RpcId rpc_id, BML_RpcHandler handler, void *user_data);
        BML_Result RegisterRpc(BML_Mod owner, BML_RpcId rpc_id, BML_RpcHandler handler,
                               void *user_data);
        BML_Result UnregisterRpc(BML_RpcId rpc_id);
        BML_Result UnregisterRpc(BML_Mod owner, BML_RpcId rpc_id);
        BML_Result CallRpc(BML_RpcId rpc_id, const BML_ImcMessage *request,
                           BML_Future *out_future);
        BML_Result CallRpc(BML_Mod owner, BML_RpcId rpc_id,
                           const BML_ImcMessage *request, BML_Future *out_future);

        BML_Result RegisterRpcEx(BML_RpcId rpc_id, BML_RpcHandlerEx handler,
                                 void *user_data);
        BML_Result RegisterRpcEx(BML_Mod owner, BML_RpcId rpc_id,
                                 BML_RpcHandlerEx handler, void *user_data);
        BML_Result CallRpcEx(BML_RpcId rpc_id, const BML_ImcMessage *request,
                             const BML_RpcCallOptions *options, BML_Future *out_future);
        BML_Result CallRpcEx(BML_Mod owner, BML_RpcId rpc_id,
                             const BML_ImcMessage *request,
                             const BML_RpcCallOptions *options, BML_Future *out_future);
        BML_Result FutureGetError(BML_Future future, BML_Result *out_code, char *msg,
                                  size_t cap, size_t *out_len);
        BML_Result GetRpcInfo(BML_RpcId rpc_id, BML_RpcInfo *out_info);
        BML_Result GetRpcName(BML_RpcId rpc_id, char *buffer, size_t cap,
                              size_t *out_len);
        void EnumerateRpc(void(*cb)(BML_RpcId, const char *, BML_Bool, void *),
                          void *user_data);
        BML_Result AddRpcMiddleware(BML_RpcMiddleware middleware, int32_t priority,
                                    void *user_data);
        BML_Result AddRpcMiddleware(BML_Mod owner, BML_RpcMiddleware middleware,
                                    int32_t priority, void *user_data);
        BML_Result RemoveRpcMiddleware(BML_RpcMiddleware middleware);
        BML_Result RemoveRpcMiddleware(BML_Mod owner, BML_RpcMiddleware middleware);
        BML_Result RegisterStreamingRpc(BML_RpcId rpc_id, BML_StreamingRpcHandler handler,
                                        void *user_data);
        BML_Result RegisterStreamingRpc(BML_Mod owner, BML_RpcId rpc_id,
                                        BML_StreamingRpcHandler handler, void *user_data);
        BML_Result StreamPush(BML_RpcStream stream, const void *data, size_t size);
        BML_Result StreamComplete(BML_RpcStream stream);
        BML_Result StreamError(BML_RpcStream stream, BML_Result error, const char *msg);
        BML_Result CallStreamingRpc(BML_RpcId rpc_id, const BML_ImcMessage *request,
                                    BML_ImcHandler on_chunk, BML_FutureCallback on_done,
                                    void *user_data, BML_Future *out_future);
        BML_Result CallStreamingRpc(BML_Mod owner, BML_RpcId rpc_id,
                                    const BML_ImcMessage *request,
                                    BML_ImcHandler on_chunk, BML_FutureCallback on_done,
                                    void *user_data, BML_Future *out_future);

        BML_Result FutureAwait(BML_Future future, uint32_t timeout_ms);
        BML_Result FutureGetResult(BML_Future future, BML_ImcMessage *out_msg);
        BML_Result FutureGetState(BML_Future future, BML_FutureState *out_state);
        BML_Result FutureCancel(BML_Future future);
        BML_Result FutureOnComplete(BML_Future future, BML_FutureCallback callback,
                                    void *user_data);
        BML_Result FutureOnComplete(BML_Mod owner, BML_Future future,
                                    BML_FutureCallback callback, void *user_data);
        BML_Result FutureRelease(BML_Future future);

        BML_Result GetStats(BML_ImcStats *out_stats);
        BML_Result ResetStats();
        BML_Result GetTopicInfo(BML_TopicId topic, BML_TopicInfo *out_info);
        BML_Result GetTopicName(BML_TopicId topic, char *buffer, size_t buffer_size,
                                size_t *out_length);
        BML_Result PublishState(BML_TopicId topic, const BML_ImcMessage *msg);
        BML_Result PublishState(BML_Mod owner, BML_TopicId topic,
                                const BML_ImcMessage *msg);
        BML_Result CopyState(BML_TopicId topic, void *dst, size_t dst_size,
                             size_t *out_size, BML_ImcStateMeta *out_meta);
        BML_Result ClearState(BML_TopicId topic);

        void Pump(size_t max_per_sub);
        void Shutdown();
        void CleanupOwner(BML_Mod owner);

    private:
        using SubscriptionPtr = BML_Subscription;

        QueuedMessage *CreateMessage(BML_Mod owner, BML_TopicId topic, const void *data,
                                     size_t size, const BML_ImcMessage *msg,
                                     const BML_ImcBuffer *buffer);
        BML_Result DispatchMessage(BML_TopicId topic, QueuedMessage *message);
        BML_Result DispatchToSubscription(BML_Subscription_T *sub, QueuedMessage *message,
                                          bool *out_enqueued);
        bool MatchesSubscription(BML_Subscription_T *sub,
                                 const BML_ImcMessage &message) const;
        void InvokeRegularHandler(BML_Subscription_T *sub, BML_TopicId topic,
                                  const BML_ImcMessage &message);
        void ReleaseMessage(QueuedMessage *message);
        void DropPendingMessages(BML_Subscription_T *sub);
        size_t DrainSubscription(BML_Subscription_T *sub, size_t budget);
        void RemoveFromMutableTopicMap(BML_TopicId topic, SubscriptionPtr handle);
        void CleanupRetiredSubscriptions();
        void ProcessRpcRequest(RpcRequest *request);
        void DrainRpcQueue(size_t budget);
        void ApplyBackpressure(BML_Subscription_T *sub, QueuedMessage *message);
        BML_Mod ResolveRegistrationOwner(BML_Mod explicit_owner = nullptr) const;
        BML_Result ResolveRegistrationOwner(BML_Mod explicit_owner,
                                            BML_Mod *out_owner) const;
        void PublishNewSnapshot();
        void RetireOldSnapshots();

        std::mutex m_WriteMutex;
        std::atomic<TopicSnapshot *> m_Snapshot{nullptr};
        std::vector<TopicSnapshot *> m_RetiredSnapshots;

        mutable std::shared_mutex m_RpcMutex;
        std::unordered_map<BML_TopicId, std::vector<SubscriptionPtr>> m_MutableTopicMap;
        std::unordered_map<SubscriptionPtr, std::unique_ptr<BML_Subscription_T>>
            m_Subscriptions;
        std::vector<std::unique_ptr<BML_Subscription_T>> m_RetiredSubscriptions;

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

        TopicRegistry m_TopicRegistry;
        TopicRegistry m_RpcRegistry;

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
} // namespace BML::Core

#endif // BML_CORE_IMC_BUS_INTERNAL_H
