#include "ImcBusInternal.h"

namespace BML::Core {
    ImcBusImpl::ImcBusImpl() {
        m_PublishState.snapshot.store(new TopicSnapshot(), std::memory_order_release);
    }

    ImcBusImpl &GetBus(KernelServices &kernel) {
        if (!kernel.imc_bus) {
            kernel.imc_bus = std::make_unique<ImcBus>();
            if (kernel.context) {
                kernel.imc_bus->BindDeps(*kernel.context);
            }
        }
        assert(kernel.imc_bus && "ImcBus not constructed");
        return kernel.imc_bus->GetImpl();
    }

    Context *ContextFromKernel(KernelServices *kernel) noexcept {
        return kernel && kernel->context ? kernel->context.get() : nullptr;
    }

    BML_Mod ImcBusImpl::ResolveRegistrationOwner(BML_Mod explicit_owner) const {
        if (!explicit_owner) {
            return nullptr;
        }
        return m_Context ? m_Context->ResolveModHandle(explicit_owner) : nullptr;
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
        auto *message = m_PublishState.message_pool.Construct<QueuedMessage>();
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
            m_PublishState.message_pool.Destroy(message);
            return nullptr;
        }

        m_GlobalStats.total_messages_published.fetch_add(1, std::memory_order_relaxed);
        m_GlobalStats.total_bytes_published.fetch_add(message->Size(),
                                                      std::memory_order_relaxed);
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
                m_GlobalStats.total_messages_dropped.fetch_add(1,
                                                               std::memory_order_relaxed);
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
            m_PublishState.message_pool.Destroy(message);
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
            std::lock_guard lock(m_PublishState.write_mutex);
            auto it = m_PublishState.retired_subscriptions.begin();
            while (it != m_PublishState.retired_subscriptions.end()) {
                if ((*it)->ref_count.load(std::memory_order_acquire) == 0) {
                    ready.push_back(std::move(*it));
                    it = m_PublishState.retired_subscriptions.erase(it);
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
        auto it = m_PublishState.mutable_topic_map.find(topic);
        if (it == m_PublishState.mutable_topic_map.end()) {
            return;
        }

        auto &subscriptions = it->second;
        subscriptions.erase(std::remove(subscriptions.begin(), subscriptions.end(), handle),
                            subscriptions.end());
        if (subscriptions.empty()) {
            m_PublishState.mutable_topic_map.erase(it);
        }
    }

    void ImcBusImpl::PublishNewSnapshot() {
        auto *snapshot = new TopicSnapshot();
        auto &registry = m_PublishState.topic_registry;

        for (auto &[topic, handles] : m_PublishState.mutable_topic_map) {
            auto &entry = snapshot->topic_subs[topic];
            entry.subs.reserve(handles.size());
            for (auto handle : handles) {
                auto subIt = m_PublishState.subscriptions.find(handle);
                if (subIt == m_PublishState.subscriptions.end()) {
                    continue;
                }

                auto *sub = subIt->second.get();
                if (!sub->closed.load(std::memory_order_acquire)) {
                    entry.subs.push_back(sub);
                }
            }
            entry.message_counter = registry.GetMessageCountPtr(topic);
        }

        snapshot->all_subs.reserve(m_PublishState.subscriptions.size());
        for (auto &[handle, sub] : m_PublishState.subscriptions) {
            (void)handle;
            if (!sub->closed.load(std::memory_order_acquire)) {
                snapshot->all_subs.push_back(sub.get());
            }
        }

        TopicSnapshot *old =
            m_PublishState.snapshot.exchange(snapshot, std::memory_order_acq_rel);
        if (old) {
            m_PublishState.retired_snapshots.push_back(old);
        }

        RetireOldSnapshots();
    }

    void ImcBusImpl::RetireOldSnapshots() {
        auto it = m_PublishState.retired_snapshots.begin();
        while (it != m_PublishState.retired_snapshots.end()) {
            if ((*it)->ref_count.load(std::memory_order_acquire) == 0) {
                delete *it;
                it = m_PublishState.retired_snapshots.erase(it);
            } else {
                ++it;
            }
        }
    }

    void ImcBusImpl::Shutdown() {
        {
            std::lock_guard lock(m_PublishState.write_mutex);
            for (auto &[handle, sub] : m_PublishState.subscriptions) {
                (void)handle;
                sub->closed.store(true, std::memory_order_release);
            }
            m_PublishState.mutable_topic_map.clear();
            PublishNewSnapshot();

            for (auto &entry : m_PublishState.subscriptions) {
                m_PublishState.retired_subscriptions.push_back(std::move(entry.second));
            }
            m_PublishState.subscriptions.clear();
        }

        for (auto &sub : m_PublishState.retired_subscriptions) {
            sub->closed.store(true, std::memory_order_release);
            while (sub->ref_count.load(std::memory_order_acquire) != 0) {
                std::this_thread::yield();
            }
            DropPendingMessages(sub.get());
        }
        m_PublishState.retired_subscriptions.clear();

        for (auto *snapshot : m_PublishState.retired_snapshots) {
            while (snapshot->ref_count.load(std::memory_order_acquire) != 0) {
                std::this_thread::yield();
            }
            delete snapshot;
        }
        m_PublishState.retired_snapshots.clear();

        {
            std::lock_guard lock(m_RetainedStateStore.state_mutex);
            m_RetainedStateStore.retained_states.clear();
        }

        {
            std::lock_guard lock(m_RpcState.stream_mutex);
            for (auto &[handle, state] : m_RpcState.streams) {
                (void)handle;
                if (state && state->future) {
                    state->future->CompleteWithError(
                        BML_FUTURE_CANCELLED, BML_RESULT_FAIL, "IMC shutdown");
                    FutureReleaseInternal(state->future);
                    state->future = nullptr;
                }
            }
            m_RpcState.streams.clear();
        }

        RpcRequest *request = nullptr;
        while (m_RpcState.rpc_queue && m_RpcState.rpc_queue->Dequeue(request)) {
            if (!request) {
                continue;
            }
            if (request->future) {
                request->future->CompleteWithError(
                    BML_FUTURE_CANCELLED, BML_RESULT_FAIL, "IMC shutdown");
                FutureReleaseInternal(request->future);
            }
            m_RpcState.rpc_request_pool.Destroy(request);
        }

        {
            std::unique_lock lock(m_RpcState.rpc_mutex);
            m_RpcState.rpc_handlers.clear();
            m_RpcState.streaming_rpc_handlers.clear();
            m_RpcState.rpc_middleware.clear();
        }
    }

    void ImcBusImpl::CleanupOwner(BML_Mod owner) {
        if (!owner) {
            return;
        }

        std::vector<BML_Subscription> subscriptions;
        {
            std::lock_guard lock(m_PublishState.write_mutex);
            subscriptions.reserve(m_PublishState.subscriptions.size());
            for (const auto &[handle, sub] : m_PublishState.subscriptions) {
                if (sub && sub->owner == owner) {
                    subscriptions.push_back(handle);
                }
            }
        }

        for (auto sub : subscriptions) {
            Unsubscribe(sub);
        }

        {
            std::unique_lock lock(m_RpcState.rpc_mutex);
            for (auto it = m_RpcState.rpc_handlers.begin();
                 it != m_RpcState.rpc_handlers.end();) {
                if (it->second.owner == owner) {
                    it = m_RpcState.rpc_handlers.erase(it);
                } else {
                    ++it;
                }
            }
            for (auto it = m_RpcState.streaming_rpc_handlers.begin();
                 it != m_RpcState.streaming_rpc_handlers.end();) {
                if (it->second.owner == owner) {
                    it = m_RpcState.streaming_rpc_handlers.erase(it);
                } else {
                    ++it;
                }
            }
            for (auto it = m_RpcState.rpc_middleware.begin();
                 it != m_RpcState.rpc_middleware.end();) {
                if (it->owner == owner) {
                    it = m_RpcState.rpc_middleware.erase(it);
                } else {
                    ++it;
                }
            }
        }

        {
            std::lock_guard lock(m_RetainedStateStore.state_mutex);
            for (auto it = m_RetainedStateStore.retained_states.begin();
                 it != m_RetainedStateStore.retained_states.end();) {
                if (it->second.owner == owner) {
                    it = m_RetainedStateStore.retained_states.erase(it);
                } else {
                    ++it;
                }
            }
        }

        {
            std::lock_guard lock(m_RpcState.stream_mutex);
            for (auto it = m_RpcState.streams.begin(); it != m_RpcState.streams.end();) {
                auto *state = it->second.get();
                if (state && state->owner == owner) {
                    if (state->future) {
                        state->future->CompleteWithError(
                            BML_FUTURE_CANCELLED, BML_RESULT_FAIL, "IMC owner cleanup");
                        FutureReleaseInternal(state->future);
                        state->future = nullptr;
                    }
                    it = m_RpcState.streams.erase(it);
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
        }
    } // namespace

    ImcBus::ImcBus()
        : m_Impl(new ImcBusImpl()),
          m_Deleter(&DeleteBusImpl) {}

    void ImcBus::BindDeps(Context &ctx) {
        GetImpl().BindDeps(ctx);
    }

    void ImcBus::Shutdown() {
        auto *impl = static_cast<ImcBusImpl *>(m_Impl);
        if (impl) {
            impl->Shutdown();
        }
    }

    ImcBusImpl &ImcBus::GetImpl() {
        return *static_cast<ImcBusImpl *>(m_Impl);
    }

    void ImcBusImpl::BindDeps(Context &ctx) {
        m_Context = &ctx;
    }
} // namespace BML::Core
