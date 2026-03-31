#include "ImcBusInternal.h"

namespace BML::Core {
    namespace {
#if defined(_MSC_VER) && !defined(__MINGW32__)
        int FilterModuleException(unsigned long code) {
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

        unsigned long InvokeHandlerSEH(BML_ImcHandler handler,
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

        unsigned long InvokeInterceptHandlerSEH(BML_ImcInterceptHandler handler,
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
    } // namespace

    BML_TopicId TopicRegistry::GetOrCreate(const char *name) {
        if (!name || !*name) {
            return BML_TOPIC_ID_INVALID;
        }

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

    const std::string *TopicRegistry::GetName(BML_TopicId id) const {
        std::shared_lock lock(m_Mutex);
        auto it = m_IdToName.find(id);
        return it != m_IdToName.end() ? &it->second : nullptr;
    }

    void TopicRegistry::IncrementMessageCount(BML_TopicId id) {
        if (id == BML_TOPIC_ID_INVALID) {
            return;
        }

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

    std::atomic<uint64_t> *TopicRegistry::GetMessageCountPtr(BML_TopicId id) {
        std::shared_lock lock(m_Mutex);
        auto it = m_Stats.find(id);
        return it != m_Stats.end() ? &it->second->message_count : nullptr;
    }

    uint64_t TopicRegistry::GetMessageCount(BML_TopicId id) const {
        std::shared_lock lock(m_Mutex);
        auto it = m_Stats.find(id);
        return it != m_Stats.end()
            ? it->second->message_count.load(std::memory_order_relaxed)
            : 0;
    }

    size_t TopicRegistry::GetTopicCount() const {
        std::shared_lock lock(m_Mutex);
        return m_IdToName.size();
    }

    void TopicRegistry::SetExpectedTypeId(BML_TopicId id, uint32_t type_id) {
        if (id == BML_TOPIC_ID_INVALID) return;
        std::unique_lock lock(m_Mutex);
        auto *stats = EnsureStatsEntryLocked(id);
        if (stats) {
            stats->expected_type_id = type_id;
        }
    }

    uint32_t TopicRegistry::GetExpectedTypeId(BML_TopicId id) const {
        std::shared_lock lock(m_Mutex);
        auto it = m_Stats.find(id);
        return it != m_Stats.end() ? it->second->expected_type_id : BML_PAYLOAD_TYPE_NONE;
    }

    TopicRegistry::TopicStats *TopicRegistry::EnsureStatsEntryLocked(BML_TopicId id) {
        if (id == BML_TOPIC_ID_INVALID) {
            return nullptr;
        }

        auto &entry = m_Stats[id];
        if (!entry) {
            entry = std::make_unique<TopicStats>();
        }
        return entry.get();
    }

    template <typename T>
    bool PriorityMessageQueue<T>::IsEmpty() const {
        for (size_t i = 0; i < kPriorityLevels; ++i) {
            if (m_Queues[i] && !m_Queues[i]->IsEmpty()) {
                return false;
            }
        }
        return true;
    }

    template <typename T>
    size_t PriorityMessageQueue<T>::ApproximateSize() const {
        size_t total = 0;
        for (size_t i = 0; i < kPriorityLevels; ++i) {
            if (m_Queues[i]) {
                total += m_Queues[i]->ApproximateSize();
            }
        }
        return total;
    }

    template <typename T>
    size_t PriorityMessageQueue<T>::Capacity() const {
        return m_CapacityPerLevel * kPriorityLevels;
    }

    template <typename T>
    size_t PriorityMessageQueue<T>::GetLevelSize(uint32_t priority) const {
        size_t level = std::min(static_cast<size_t>(priority), kPriorityLevels - 1);
        return m_Queues[level] ? m_Queues[level]->ApproximateSize() : 0;
    }

    template class PriorityMessageQueue<QueuedMessage *>;

    BML_Result ImcBusImpl::GetTopicId(const char *name, BML_TopicId *out_id) {
        if (!name || !*name || !out_id)
            return BML_RESULT_INVALID_ARGUMENT;
        *out_id = m_PublishState.topic_registry.GetOrCreate(name);
        return (*out_id != BML_TOPIC_ID_INVALID) ? BML_RESULT_OK : BML_RESULT_FAIL;
    }

    BML_Result ImcBusImpl::DispatchToSubscription(BML_Subscription_T *sub,
                                                  QueuedMessage *message,
                                                  bool *out_enqueued) {
        if (out_enqueued) {
            *out_enqueued = false;
        }
        if (!sub || !message)
            return BML_RESULT_INVALID_ARGUMENT;

        // Fast path: skip filter_msg construction when no filter and priority matches.
        // Covers >95% of dispatches (most subscriptions use default priority and no filter).
        if (sub->filter || message->priority < sub->min_priority) {
            BML_ImcMessage filter_msg = {};
            filter_msg.struct_size = sizeof(BML_ImcMessage);
            filter_msg.data = message->Data();
            filter_msg.size = message->Size();
            filter_msg.msg_id = message->msg_id;
            filter_msg.flags = message->flags;
            filter_msg.priority = message->priority;
            filter_msg.timestamp = message->timestamp;
            filter_msg.reply_topic = message->reply_topic;
            filter_msg.payload_type_id = message->payload_type_id;

            if (!MatchesSubscription(sub, filter_msg))
                return BML_RESULT_OK;
        }

        if (sub->queue && sub->queue->Enqueue(message, message->priority)) {
            sub->has_pending.store(true, std::memory_order_release);
            sub->RecordReceived(message->Size());
            m_GlobalStats.total_messages_delivered.fetch_add(1,
                                                             std::memory_order_relaxed);
            if (out_enqueued) {
                *out_enqueued = true;
            }
            return BML_RESULT_OK;
        }

        switch (sub->backpressure_policy) {
        case BML_BACKPRESSURE_DROP_OLDEST:
            ApplyBackpressure(sub, message);
            if (sub->queue && sub->queue->Enqueue(message, message->priority)) {
                sub->has_pending.store(true, std::memory_order_release);
                sub->RecordReceived(message->Size());
                m_GlobalStats.total_messages_delivered.fetch_add(
                    1, std::memory_order_relaxed);
                if (out_enqueued) {
                    *out_enqueued = true;
                }
                return BML_RESULT_OK;
            }
            break;
        case BML_BACKPRESSURE_DROP_NEWEST:
            sub->RecordDropped();
            m_GlobalStats.total_messages_dropped.fetch_add(1,
                                                           std::memory_order_relaxed);
            return BML_RESULT_OK;
        case BML_BACKPRESSURE_BLOCK:
            for (int spin = 0; spin < 100; ++spin) {
                std::this_thread::yield();
                if (sub->queue && sub->queue->Enqueue(message, message->priority)) {
                    sub->has_pending.store(true, std::memory_order_release);
                    sub->RecordReceived(message->Size());
                    m_GlobalStats.total_messages_delivered.fetch_add(
                        1, std::memory_order_relaxed);
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
        m_GlobalStats.total_messages_dropped.fetch_add(1, std::memory_order_relaxed);
        return BML_RESULT_OK;
    }

    bool ImcBusImpl::MatchesSubscription(BML_Subscription_T *sub,
                                         const BML_ImcMessage &message) const {
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

        auto *busContext = m_Context;
        BML_Context ctx = busContext ? busContext->GetHandle() : nullptr;

        try {
            DispatchSubscriptionScope dispatch_scope(sub);
            if (dispatch_scope.Overflowed())
                return;
            ModuleContextScope module_scope(sub->owner);
#if defined(_MSC_VER) && !defined(__MINGW32__)
            BML_ImcMessage mutable_message = message;
            unsigned long seh_code =
                InvokeHandlerSEH(sub->handler, ctx, topic, &mutable_message, sub->user_data);
            if (seh_code != 0) {
                std::string owner_id;
                if (sub->owner) {
                    auto *mod = busContext ? busContext->ResolveModHandle(sub->owner) : nullptr;
                    if (mod) {
                        owner_id = mod->id;
                    }
                }
                CoreLog(BML_LOG_ERROR, kImcLogCategory,
                        "Subscriber crashed (code 0x%08lX) on topic %u, owned by module '%s' "
                        "-- unsubscribing",
                        seh_code, static_cast<unsigned>(topic),
                        owner_id.empty() ? "unknown" : owner_id.c_str());

                if (busContext) {
                    busContext->GetCrashDump().WriteDumpOnce(owner_id, seh_code);
                }
                if (!owner_id.empty()) {
                    if (busContext) {
                        busContext->GetFaultTracker().RecordFault(owner_id, seh_code);
                    }
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
        }
    }

    BML_Result ImcBusImpl::DispatchMessage(BML_TopicId topic, QueuedMessage *message) {
        if (!message)
            return BML_RESULT_OUT_OF_MEMORY;

        SnapshotGuard guard(m_PublishState.snapshot.load(std::memory_order_acquire));
        if (!guard) {
            m_PublishState.topic_registry.IncrementMessageCount(topic);
            ReleaseMessage(message);
            return BML_RESULT_OK;
        }

        auto it = guard.get()->topic_subs.find(topic);
        if (it == guard.get()->topic_subs.end() || it->second.subs.empty()) {
            m_PublishState.topic_registry.IncrementMessageCount(topic);
            ReleaseMessage(message);
            return BML_RESULT_OK;
        }

        const auto &entry = it->second;
        if (entry.message_counter) {
            entry.message_counter->fetch_add(1, std::memory_order_relaxed);
        } else {
            m_PublishState.topic_registry.IncrementMessageCount(topic);
        }

        StackVec<BML_Subscription_T *, 16> targets;
        for (auto *sub : entry.subs) {
            if (sub->closed.load(std::memory_order_acquire) || !sub->handler || !sub->queue)
                continue;
            targets.push_back(sub);
        }

        if (targets.empty()) {
            ReleaseMessage(message);
            return BML_RESULT_OK;
        }

        message->ref_count.store(static_cast<uint32_t>(targets.size()) + 1,
                                 std::memory_order_release);
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

        ReleaseMessage(message);
        if (delivered == 0 && first_error != BML_RESULT_OK) {
            return first_error;
        }
        return BML_RESULT_OK;
    }

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
                imc_msg.payload_type_id = msg->payload_type_id;
                InvokeRegularHandler(sub, msg->topic_id, imc_msg);
            }
            ReleaseMessage(msg);
            ++processed;
        }
        return processed;
    }

    BML_Result ImcBusImpl::Publish(BML_TopicId topic, const void *data, size_t size,
                                   uint32_t type_id) {
        return Publish(nullptr, topic, data, size, type_id);
    }

    BML_Result ImcBusImpl::Publish(BML_Mod owner, BML_TopicId topic,
                                   const void *data, size_t size,
                                   uint32_t type_id) {
        if (topic == BML_TOPIC_ID_INVALID)
            return BML_RESULT_INVALID_ARGUMENT;
        if (size > 0 && !data)
            return BML_RESULT_INVALID_ARGUMENT;

        // Type validation: if message carries a type_id AND topic has an expected type,
        // they must match
        if (type_id != BML_PAYLOAD_TYPE_NONE) {
            uint32_t expected = m_PublishState.topic_registry.GetExpectedTypeId(topic);
            if (expected != BML_PAYLOAD_TYPE_NONE && expected != type_id) {
                const std::string *name = m_PublishState.topic_registry.GetName(topic);
                CoreLog(BML_LOG_ERROR, kImcLogCategory,
                        "Type mismatch on topic '%s': expected 0x%08X, got 0x%08X",
                        name ? name->c_str() : "<unknown>", expected, type_id);
                return BML_RESULT_IMC_TYPE_MISMATCH;
            }
        }

        BML_Mod resolved_owner = nullptr;
        BML_CHECK(ResolveRegistrationOwner(owner, &resolved_owner));

        BML_ImcMessage msg = BML_IMC_MESSAGE_INIT;
        msg.data = data;
        msg.size = size;
        msg.payload_type_id = type_id;

        FireTap(resolved_owner, topic, &msg);

        auto *message = CreateMessage(resolved_owner, topic, data, size, &msg, nullptr);
        return DispatchMessage(topic, message);
    }

    BML_Result ImcBusImpl::PublishEx(BML_TopicId topic, const BML_ImcMessage *msg) {
        return PublishEx(nullptr, topic, msg);
    }

    BML_Result ImcBusImpl::PublishEx(BML_Mod owner, BML_TopicId topic,
                                     const BML_ImcMessage *msg) {
        if (topic == BML_TOPIC_ID_INVALID || !msg)
            return BML_RESULT_INVALID_ARGUMENT;
        if (msg->size > 0 && !msg->data)
            return BML_RESULT_INVALID_ARGUMENT;

        // Type validation
        if (msg->payload_type_id != BML_PAYLOAD_TYPE_NONE) {
            uint32_t expected = m_PublishState.topic_registry.GetExpectedTypeId(topic);
            if (expected != BML_PAYLOAD_TYPE_NONE && expected != msg->payload_type_id) {
                const std::string *name = m_PublishState.topic_registry.GetName(topic);
                CoreLog(BML_LOG_ERROR, kImcLogCategory,
                        "Type mismatch on topic '%s': expected 0x%08X, got 0x%08X",
                        name ? name->c_str() : "<unknown>", expected, msg->payload_type_id);
                return BML_RESULT_IMC_TYPE_MISMATCH;
            }
        }

        BML_Mod resolved_owner = nullptr;
        BML_CHECK(ResolveRegistrationOwner(owner, &resolved_owner));

        FireTap(resolved_owner, topic, msg);

        auto *message = CreateMessage(resolved_owner, topic, msg->data, msg->size, msg, nullptr);
        return DispatchMessage(topic, message);
    }

    BML_Result ImcBusImpl::PublishBuffer(BML_TopicId topic, const BML_ImcBuffer *buffer) {
        return PublishBuffer(nullptr, topic, buffer);
    }

    BML_Result ImcBusImpl::PublishBuffer(BML_Mod owner, BML_TopicId topic,
                                         const BML_ImcBuffer *buffer) {
        if (topic == BML_TOPIC_ID_INVALID || !buffer)
            return BML_RESULT_INVALID_ARGUMENT;
        if (buffer->size > 0 && !buffer->data)
            return BML_RESULT_INVALID_ARGUMENT;

        BML_Mod resolved_owner = nullptr;
        BML_CHECK(ResolveRegistrationOwner(owner, &resolved_owner));

        BML_ImcMessage msg = BML_IMC_MESSAGE_INIT;
        msg.data = buffer->data;
        msg.size = buffer->size;
        FireTap(resolved_owner, topic, &msg);

        auto *message = CreateMessage(resolved_owner, topic, nullptr, 0, nullptr, buffer);
        return DispatchMessage(topic, message);
    }

    BML_Result ImcBusImpl::Subscribe(BML_TopicId topic,
                                     BML_ImcHandler handler,
                                     void *user_data,
                                     BML_Subscription *out_sub) {
        BML_SubscribeOptions opts = BML_SUBSCRIBE_OPTIONS_INIT;
        return SubscribeEx(nullptr, topic, handler, user_data, &opts, out_sub);
    }

    BML_Result ImcBusImpl::Subscribe(BML_Mod owner,
                                     BML_TopicId topic,
                                     BML_ImcHandler handler,
                                     void *user_data,
                                     BML_Subscription *out_sub) {
        BML_SubscribeOptions opts = BML_SUBSCRIBE_OPTIONS_INIT;
        return SubscribeEx(owner, topic, handler, user_data, &opts, out_sub);
    }

    BML_Result ImcBusImpl::SubscribeEx(BML_TopicId topic,
                                       BML_ImcHandler handler,
                                       void *user_data,
                                       const BML_SubscribeOptions *options,
                                       BML_Subscription *out_sub) {
        return SubscribeEx(nullptr, topic, handler, user_data, options, out_sub);
    }

    BML_Result ImcBusImpl::SubscribeEx(BML_Mod owner,
                                       BML_TopicId topic,
                                       BML_ImcHandler handler,
                                       void *user_data,
                                       const BML_SubscribeOptions *options,
                                       BML_Subscription *out_sub) {
        if (topic == BML_TOPIC_ID_INVALID || !handler || !out_sub)
            return BML_RESULT_INVALID_ARGUMENT;

        BML_Mod resolved_owner = nullptr;
        BML_CHECK(ResolveRegistrationOwner(owner, &resolved_owner));

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
                capacity = options->queue_capacity > 0
                    ? options->queue_capacity
                    : kDefaultQueueCapacity;
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
        subscription->kernel = m_Context ? Context::KernelFromHandle(m_Context->GetHandle()) : nullptr;
        subscription->owner = resolved_owner;
        subscription->queue_capacity = capacity;
        subscription->min_priority = min_priority;
        subscription->backpressure_policy = backpressure;
        subscription->filter = filter;
        subscription->filter_user_data = filter_user_data;
        subscription->InitStats();
        subscription->queue =
            std::make_unique<PriorityMessageQueue<QueuedMessage *>>(capacity);
        if (!subscription->queue)
            return BML_RESULT_OUT_OF_MEMORY;

        BML_Subscription handle = subscription.get();
        {
            std::lock_guard lock(m_PublishState.write_mutex);
            m_PublishState.mutable_topic_map[topic].push_back(handle);
            m_PublishState.subscriptions.emplace(handle, std::move(subscription));
            PublishNewSnapshot();
        }

        *out_sub = handle;

        if ((subscribe_flags & BML_IMC_SUBSCRIBE_FLAG_DELIVER_RETAINED_ON_SUBSCRIBE) != 0) {
            RetainedStateEntry retained;
            bool has_retained = false;
            {
                std::lock_guard<std::mutex> state_lock(m_RetainedStateStore.state_mutex);
                auto state_it = m_RetainedStateStore.retained_states.find(topic);
                if (state_it != m_RetainedStateStore.retained_states.end()) {
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
                    m_GlobalStats.total_messages_delivered.fetch_add(
                        1, std::memory_order_relaxed);
                    handle->ref_count.fetch_add(1, std::memory_order_relaxed);
                    InvokeRegularHandler(handle, topic, retained_msg);
                    handle->ref_count.fetch_sub(1, std::memory_order_acq_rel);
                }
            }
        }

        return BML_RESULT_OK;
    }

    BML_Result ImcBusImpl::SubscribeIntercept(BML_TopicId topic,
                                              BML_ImcInterceptHandler handler,
                                              void *user_data,
                                              BML_Subscription *out_sub) {
        return SubscribeInterceptEx(nullptr, topic, handler, user_data, nullptr, out_sub);
    }

    BML_Result ImcBusImpl::SubscribeIntercept(BML_Mod owner,
                                              BML_TopicId topic,
                                              BML_ImcInterceptHandler handler,
                                              void *user_data,
                                              BML_Subscription *out_sub) {
        return SubscribeInterceptEx(owner, topic, handler, user_data, nullptr, out_sub);
    }

    BML_Result ImcBusImpl::SubscribeInterceptEx(BML_TopicId topic,
                                                BML_ImcInterceptHandler handler,
                                                void *user_data,
                                                const BML_SubscribeOptions *options,
                                                BML_Subscription *out_sub) {
        return SubscribeInterceptEx(nullptr, topic, handler, user_data, options, out_sub);
    }

    BML_Result ImcBusImpl::SubscribeInterceptEx(BML_Mod owner,
                                                BML_TopicId topic,
                                                BML_ImcInterceptHandler handler,
                                                void *user_data,
                                                const BML_SubscribeOptions *options,
                                                BML_Subscription *out_sub) {
        if (topic == BML_TOPIC_ID_INVALID || !handler || !out_sub)
            return BML_RESULT_INVALID_ARGUMENT;

        BML_Mod resolved_owner = nullptr;
        BML_CHECK(ResolveRegistrationOwner(owner, &resolved_owner));

        int32_t exec_order = 0;
        if (options && options->struct_size >=
                offsetof(BML_SubscribeOptions, execution_order) + sizeof(int32_t)) {
            exec_order = options->execution_order;
        }

        auto subscription = std::make_unique<BML_Subscription_T>();
        subscription->topic_id = topic;
        subscription->intercept_handler = handler;
        subscription->user_data = user_data;
        subscription->kernel = m_Context ? Context::KernelFromHandle(m_Context->GetHandle()) : nullptr;
        subscription->owner = resolved_owner;
        subscription->queue_capacity = 0;
        subscription->execution_order = exec_order;
        subscription->InitStats();

        BML_Subscription handle = subscription.get();
        {
            std::lock_guard lock(m_PublishState.write_mutex);
            m_PublishState.mutable_topic_map[topic].push_back(handle);
            m_PublishState.subscriptions.emplace(handle, std::move(subscription));
            PublishNewSnapshot();
        }

        *out_sub = handle;
        return BML_RESULT_OK;
    }

    BML_Result ImcBusImpl::PublishInterceptable(BML_TopicId topic,
                                                BML_ImcMessage *msg,
                                                BML_EventResult *out_result) {
        return PublishInterceptable(nullptr, topic, msg, out_result);
    }

    BML_Result ImcBusImpl::PublishInterceptable(BML_Mod owner,
                                                BML_TopicId topic,
                                                BML_ImcMessage *msg,
                                                BML_EventResult *out_result) {
        if (topic == BML_TOPIC_ID_INVALID || !msg)
            return BML_RESULT_INVALID_ARGUMENT;

        BML_Mod resolved_owner = nullptr;
        BML_CHECK(ResolveRegistrationOwner(owner, &resolved_owner));

        FireTap(resolved_owner, topic, msg);

        BML_EventResult final_result = BML_EVENT_CONTINUE;
        auto *busContext = m_Context;
        BML_Context ctx = busContext ? busContext->GetHandle() : nullptr;

        StackVec<BML_Subscription_T *, 8> interceptors;
        {
            SnapshotGuard guard(m_PublishState.snapshot.load(std::memory_order_acquire));
            if (guard) {
                auto it = guard.get()->topic_subs.find(topic);
                if (it != guard.get()->topic_subs.end()) {
                    const auto &subs = it->second.subs;
                    for (size_t i = 0; i < subs.size(); ++i) {
                        BML_Subscription_T *sub = subs[i];
                        if (sub && !sub->closed.load(std::memory_order_acquire) &&
                            sub->IsIntercept()) {
                            sub->ref_count.fetch_add(1, std::memory_order_relaxed);
                            interceptors.push_back(sub);
                        }
                    }
                }
            }
        }

        if (!interceptors.empty()) {
            std::stable_sort(interceptors.begin(), interceptors.end(),
                             [](const BML_Subscription_T *a,
                                const BML_Subscription_T *b) {
                                 return a->execution_order < b->execution_order;
                             });

            bool stopped = false;
            for (size_t idx = 0; idx < interceptors.size(); ++idx) {
                BML_Subscription_T *sub = interceptors[idx];
                if (!stopped && !sub->closed.load(std::memory_order_acquire) &&
                    sub->intercept_handler) {
                    BML_EventResult result = BML_EVENT_CONTINUE;
                    try {
                        DispatchSubscriptionScope dispatch_scope(sub);
                        if (dispatch_scope.Overflowed())
                            continue;
                        ModuleContextScope module_scope(sub->owner);
#if defined(_MSC_VER) && !defined(__MINGW32__)
                        unsigned long seh_code = InvokeInterceptHandlerSEH(
                            sub->intercept_handler, ctx, topic, msg, sub->user_data, &result);
                        if (seh_code != 0) {
                            std::string owner_id;
                            if (sub->owner) {
                                auto *mod = busContext ? busContext->ResolveModHandle(sub->owner) : nullptr;
                                if (mod) {
                                    owner_id = mod->id;
                                }
                            }
                            CoreLog(BML_LOG_ERROR, kImcLogCategory,
                                    "Intercept handler crashed (code 0x%08lX) on topic %u, "
                                    "owned by module '%s' -- unsubscribing",
                                    seh_code, static_cast<unsigned>(topic),
                                    owner_id.empty() ? "unknown" : owner_id.c_str());
                            if (busContext) {
                                busContext->GetCrashDump().WriteDumpOnce(owner_id, seh_code);
                            }
                            if (!owner_id.empty()) {
                                if (busContext) {
                                    busContext->GetFaultTracker().RecordFault(owner_id, seh_code);
                                }
                            }
                            sub->closed.store(true, std::memory_order_release);
                            result = BML_EVENT_CONTINUE;
                        } else {
                            sub->RecordProcessed();
                        }
#else
                        result = sub->intercept_handler(ctx, topic, msg, sub->user_data);
                        sub->RecordProcessed();
#endif
                    } catch (...) {
                    }
                    if (result == BML_EVENT_HANDLED || result == BML_EVENT_CANCEL) {
                        final_result = result;
                        stopped = true;
                    }
                }
                sub->ref_count.fetch_sub(1, std::memory_order_acq_rel);
            }
        }

        if (final_result != BML_EVENT_CANCEL) {
            BML_Result pub_result = PublishEx(resolved_owner, topic, msg);
            if (pub_result != BML_RESULT_OK && pub_result != BML_RESULT_NOT_FOUND) {
                if (out_result) {
                    *out_result = BML_EVENT_CONTINUE;
                }
                return pub_result;
            }
        }

        if (out_result) {
            *out_result = final_result;
        }
        return BML_RESULT_OK;
    }

    BML_Result ImcBusImpl::GetSubscriptionStats(BML_Subscription sub,
                                                BML_SubscriptionStats *out_stats) {
        if (!sub)
            return BML_RESULT_INVALID_HANDLE;
        if (!out_stats)
            return BML_RESULT_INVALID_ARGUMENT;

        std::lock_guard lock(m_PublishState.write_mutex);
        auto it = m_PublishState.subscriptions.find(sub);
        if (it == m_PublishState.subscriptions.end())
            return BML_RESULT_INVALID_HANDLE;

        it->second->FillStats(out_stats);
        return BML_RESULT_OK;
    }

    BML_Result ImcBusImpl::PublishMulti(const BML_TopicId *topics,
                                        size_t topic_count,
                                        const void *data,
                                        size_t size,
                                        const BML_ImcMessage *msg,
                                        size_t *out_delivered) {
        return PublishMulti(nullptr, topics, topic_count, data, size, msg, out_delivered);
    }

    BML_Result ImcBusImpl::PublishMulti(BML_Mod owner,
                                        const BML_TopicId *topics,
                                        size_t topic_count,
                                        const void *data,
                                        size_t size,
                                        const BML_ImcMessage *msg,
                                        size_t *out_delivered) {
        if (!topics || topic_count == 0)
            return BML_RESULT_INVALID_ARGUMENT;
        if (size > 0 && !data)
            return BML_RESULT_INVALID_ARGUMENT;

        BML_Mod resolved_owner = nullptr;
        BML_CHECK(ResolveRegistrationOwner(owner, &resolved_owner));

        BML_ImcMessage trace_msg = BML_IMC_MESSAGE_INIT;
        if (msg) {
            trace_msg = *msg;
        } else {
            trace_msg.data = data;
            trace_msg.size = size;
        }

        size_t delivered = 0;
        BML_Result first_error = BML_RESULT_OK;
        for (size_t i = 0; i < topic_count; ++i) {
            if (topics[i] == BML_TOPIC_ID_INVALID)
                continue;

            FireTap(resolved_owner, topics[i], &trace_msg);

            auto *message = CreateMessage(resolved_owner, topics[i], data, size, msg, nullptr);
            BML_Result res = DispatchMessage(topics[i], message);
            if (res == BML_RESULT_OK) {
                ++delivered;
            } else if (first_error == BML_RESULT_OK) {
                first_error = res;
            }
        }

        if (out_delivered) {
            *out_delivered = delivered;
        }
        return delivered > 0 ? BML_RESULT_OK : first_error;
    }

    BML_Result ImcBusImpl::Unsubscribe(BML_Subscription sub) {
        if (!sub)
            return BML_RESULT_INVALID_HANDLE;

        std::unique_ptr<BML_Subscription_T> owned;
        BML_Subscription_T *raw = nullptr;
        {
            std::lock_guard lock(m_PublishState.write_mutex);
            auto it = m_PublishState.subscriptions.find(sub);
            if (it == m_PublishState.subscriptions.end())
                return BML_RESULT_INVALID_HANDLE;

            auto *s = it->second.get();
            s->closed.store(true, std::memory_order_release);
            RemoveFromMutableTopicMap(s->topic_id, sub);
            owned = std::move(it->second);
            raw = owned.get();
            m_PublishState.subscriptions.erase(it);
            PublishNewSnapshot();
        }

        if (raw) {
            if (std::find(g_DispatchSubscriptionStack.begin(),
                          g_DispatchSubscriptionStack.end(),
                          raw) != g_DispatchSubscriptionStack.end()) {
                std::lock_guard lock(m_PublishState.write_mutex);
                m_PublishState.retired_subscriptions.push_back(std::move(owned));
                return BML_RESULT_OK;
            }

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
                std::lock_guard lock(m_PublishState.write_mutex);
                m_PublishState.retired_subscriptions.push_back(std::move(owned));
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

        std::lock_guard lock(m_PublishState.write_mutex);
        auto it = m_PublishState.subscriptions.find(sub);
        if (it == m_PublishState.subscriptions.end()) {
            *out_active = BML_FALSE;
            return BML_RESULT_INVALID_HANDLE;
        }

        *out_active =
            it->second->closed.load(std::memory_order_acquire) ? BML_FALSE : BML_TRUE;
        return BML_RESULT_OK;
    }

    void ImcBusImpl::Pump(size_t max_per_sub) {
        const uint64_t pumpTimestampNs = GetTimestampNsRaw();

        m_GlobalStats.pump_cycles.fetch_add(1, std::memory_order_relaxed);
        m_GlobalStats.last_pump_time.store(pumpTimestampNs, std::memory_order_relaxed);

        DrainRpcQueue(max_per_sub);

        {
            SnapshotGuard guard(m_PublishState.snapshot.load(std::memory_order_acquire));
            if (!guard) {
                return;
            }

            for (auto *sub : guard.get()->all_subs) {
                if (sub->closed.load(std::memory_order_acquire))
                    continue;
                // Fast skip: no pending messages means empty queue, avoid drain overhead.
                if (!sub->has_pending.exchange(false, std::memory_order_acq_rel))
                    continue;
                sub->ref_count.fetch_add(1, std::memory_order_relaxed);
                DrainSubscription(sub, max_per_sub);
                sub->ref_count.fetch_sub(1, std::memory_order_acq_rel);
                if (sub->queue && !sub->queue->IsEmpty()) {
                    sub->has_pending.store(true, std::memory_order_release);
                }
            }
        }

        // Run cleanup tasks every 64th pump cycle instead of every frame.
        uint64_t cycles = m_GlobalStats.pump_cycles.load(std::memory_order_relaxed);
        if ((cycles & 63) == 0) {
            CleanupRetiredSubscriptions();
            {
                std::lock_guard lock(m_PublishState.write_mutex);
                RetireOldSnapshots();
            }
        }
    }
} // namespace BML::Core
