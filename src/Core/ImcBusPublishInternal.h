#ifndef BML_CORE_IMC_BUS_PUBLISH_INTERNAL_H
#define BML_CORE_IMC_BUS_PUBLISH_INTERNAL_H

#include "ImcBusSharedInternal.h"

#include <vector>

#include "FixedBlockPool.h"
#include "Logging.h"

namespace BML::Core {
    struct KernelServices;

    struct QueuedMessage {
        BML_TopicId topic_id{0};
        BML_Mod sender{nullptr};
        uint64_t msg_id{0};
        uint32_t flags{0};
        uint32_t priority{BML_IMC_PRIORITY_NORMAL};
        uint64_t timestamp{0};
        BML_TopicId reply_topic{0};
        uint32_t payload_type_id{BML_PAYLOAD_TYPE_NONE};
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
} // namespace BML::Core

struct BML_Subscription_T {
    BML_TopicId topic_id{0};
    BML_ImcHandler handler{nullptr};
    BML_ImcInterceptHandler intercept_handler{nullptr};
    void *user_data{nullptr};
    BML::Core::KernelServices *kernel{nullptr};
    BML_Mod owner{nullptr};
    std::atomic<uint32_t> ref_count{0};
    std::atomic<bool> closed{false};
    std::atomic<bool> has_pending{false};  // Set on enqueue, cleared after drain

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

namespace BML::Core {
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

    struct PublishRuntimeState {
        std::mutex write_mutex;
        std::atomic<TopicSnapshot *> snapshot{nullptr};
        std::vector<TopicSnapshot *> retired_snapshots;
        std::unordered_map<BML_TopicId, std::vector<BML_Subscription>> mutable_topic_map;
        std::unordered_map<BML_Subscription, std::unique_ptr<BML_Subscription_T>> subscriptions;
        std::vector<std::unique_ptr<BML_Subscription_T>> retired_subscriptions;
        FixedBlockPool message_pool{sizeof(QueuedMessage)};
        TopicRegistry topic_registry;
    };
} // namespace BML::Core

#endif // BML_CORE_IMC_BUS_PUBLISH_INTERNAL_H
