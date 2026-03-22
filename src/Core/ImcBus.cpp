#include "ImcBusInternal.h"

namespace BML::Core {
    Context *g_BusContext = nullptr;
    ImcBusImpl *g_BusPtr = nullptr;

    ImcBusImpl::ImcBusImpl()
        : m_MessagePool(sizeof(QueuedMessage)),
          m_RpcRequestPool(sizeof(RpcRequest)),
          m_RpcQueue(std::make_unique<MpscRingBuffer<RpcRequest *>>(kDefaultRpcQueueCapacity)) {
        m_Snapshot.store(new TopicSnapshot(), std::memory_order_release);
    }

    ImcBusImpl &GetBus() {
        assert(g_BusPtr && "ImcBus not constructed");
        return *g_BusPtr;
    }

    BML_Mod ImcBusImpl::ResolveRegistrationOwner(BML_Mod explicit_owner) const {
        if (!explicit_owner) {
            return nullptr;
        }
        return g_BusContext->ResolveModHandle(explicit_owner);
    }

    BML_Result ImcBusImpl::ResolveRegistrationOwner(BML_Mod explicit_owner,
                                                    BML_Mod *out_owner) const {
        if (!out_owner) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        *out_owner = ResolveRegistrationOwner(explicit_owner);
        return *out_owner ? BML_RESULT_OK : BML_RESULT_INVALID_CONTEXT;
    }

    QueuedMessage *ImcBusImpl::CreateMessage(BML_Mod owner,
                                             BML_TopicId topic,
                                             const void *data,
                                             size_t size,
                                             const BML_ImcMessage *msg,
                                             const BML_ImcBuffer *buffer) {
        auto *message = m_MessagePool.Construct<QueuedMessage>();
        if (!message) {
            return nullptr;
        }

        message->topic_id = topic;
        message->sender = owner;
        message->msg_id = msg && msg->msg_id
            ? msg->msg_id
            : m_NextMessageId.fetch_add(1, std::memory_order_relaxed);
        message->flags = msg ? msg->flags : 0;
        message->priority = msg ? msg->priority : BML_IMC_PRIORITY_NORMAL;
        message->timestamp = (msg && msg->timestamp) ? msg->timestamp : GetTimestampNs();
        message->reply_topic = msg ? msg->reply_topic : 0;

        const bool ok = buffer
            ? message->SetExternalBuffer(*buffer)
            : message->SetPayload(data, size);
        if (!ok) {
            m_MessagePool.Destroy(message);
            return nullptr;
        }

        m_Stats.total_messages_published.fetch_add(1, std::memory_order_relaxed);
        m_Stats.total_bytes_published.fetch_add(message->Size(), std::memory_order_relaxed);
        return message;
    }

    void ImcBusImpl::ApplyBackpressure(BML_Subscription_T *sub, QueuedMessage *message) {
        if (!sub || !sub->queue || !message) {
            return;
        }

        switch (sub->backpressure_policy) {
        case BML_BACKPRESSURE_DROP_OLDEST: {
            QueuedMessage *old = nullptr;
            if (sub->queue->Dequeue(old)) {
                ReleaseMessage(old);
                sub->RecordDropped();
                m_Stats.total_messages_dropped.fetch_add(1, std::memory_order_relaxed);
            }
            break;
        }
        case BML_BACKPRESSURE_DROP_NEWEST:
        case BML_BACKPRESSURE_BLOCK:
        case BML_BACKPRESSURE_FAIL:
        default:
            break;
        }
    }

    void ImcBusImpl::ReleaseMessage(QueuedMessage *message) {
        if (!message) {
            return;
        }

        const uint32_t prev = message->ref_count.fetch_sub(1, std::memory_order_acq_rel);
        if (prev == 1) {
            m_MessagePool.Destroy(message);
        }
    }

    void ImcBusImpl::DropPendingMessages(BML_Subscription_T *sub) {
        if (!sub || !sub->queue) {
            return;
        }

        QueuedMessage *message = nullptr;
        while (sub->queue->Dequeue(message)) {
            ReleaseMessage(message);
        }
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
        if (it == m_MutableTopicMap.end()) {
            return;
        }

        auto &subscriptions = it->second;
        subscriptions.erase(std::remove(subscriptions.begin(), subscriptions.end(), handle),
                            subscriptions.end());
        if (subscriptions.empty()) {
            m_MutableTopicMap.erase(it);
        }
    }

    void ImcBusImpl::PublishNewSnapshot() {
        auto *snapshot = new TopicSnapshot();
        auto &registry = m_TopicRegistry;

        for (auto &[topic, handles] : m_MutableTopicMap) {
            auto &entry = snapshot->topic_subs[topic];
            entry.subs.reserve(handles.size());
            for (auto handle : handles) {
                auto subIt = m_Subscriptions.find(handle);
                if (subIt == m_Subscriptions.end()) {
                    continue;
                }

                auto *sub = subIt->second.get();
                if (!sub->closed.load(std::memory_order_acquire)) {
                    entry.subs.push_back(sub);
                }
            }
            entry.message_counter = registry.GetMessageCountPtr(topic);
        }

        snapshot->all_subs.reserve(m_Subscriptions.size());
        for (auto &[handle, sub] : m_Subscriptions) {
            (void)handle;
            if (!sub->closed.load(std::memory_order_acquire)) {
                snapshot->all_subs.push_back(sub.get());
            }
        }

        TopicSnapshot *old = m_Snapshot.exchange(snapshot, std::memory_order_acq_rel);
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

    void ImcBusImpl::Shutdown() {
        {
            std::lock_guard lock(m_WriteMutex);
            for (auto &[handle, sub] : m_Subscriptions) {
                (void)handle;
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

        for (auto *snapshot : m_RetiredSnapshots) {
            while (snapshot->ref_count.load(std::memory_order_acquire) != 0) {
                std::this_thread::yield();
            }
            delete snapshot;
        }
        m_RetiredSnapshots.clear();

        {
            std::lock_guard lock(m_StateMutex);
            m_RetainedStates.clear();
        }

        {
            std::lock_guard lock(m_StreamMutex);
            for (auto &[handle, state] : m_Streams) {
                (void)handle;
                if (state && state->future) {
                    state->future->CompleteWithError(
                        BML_FUTURE_CANCELLED, BML_RESULT_FAIL, "IMC shutdown");
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
                request->future->CompleteWithError(
                    BML_FUTURE_CANCELLED, BML_RESULT_FAIL, "IMC shutdown");
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
            for (auto it = m_StreamingRpcHandlers.begin();
                 it != m_StreamingRpcHandlers.end();) {
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
            std::lock_guard lock(m_StateMutex);
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
                        state->future->CompleteWithError(
                            BML_FUTURE_CANCELLED, BML_RESULT_FAIL, "IMC owner cleanup");
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

    namespace {
        void DeleteBusImpl(void *p) {
            auto *impl = static_cast<ImcBusImpl *>(p);
            if (impl) {
                impl->Shutdown();
            }
            delete impl;
            g_BusPtr = nullptr;
            g_BusContext = nullptr;
        }
    } // namespace

    ImcBus::ImcBus()
        : m_Impl(new ImcBusImpl()),
          m_Deleter(&DeleteBusImpl) {
        g_BusPtr = static_cast<ImcBusImpl *>(m_Impl);
    }

    void ImcBus::BindDeps(Context &ctx) {
        g_BusContext = &ctx;
    }

    void ImcBus::Shutdown() {
        if (g_BusPtr) {
            g_BusPtr->Shutdown();
        }
    }
} // namespace BML::Core
