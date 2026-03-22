#include "ImcBusInternal.h"

namespace BML::Core {
    BML_Result ImcBusImpl::GetStats(BML_ImcStats *out_stats) {
        if (!out_stats)
            return BML_RESULT_INVALID_ARGUMENT;

        out_stats->struct_size = sizeof(BML_ImcStats);
        out_stats->total_messages_published =
            m_Stats.total_messages_published.load(std::memory_order_relaxed);
        out_stats->total_messages_delivered =
            m_Stats.total_messages_delivered.load(std::memory_order_relaxed);
        out_stats->total_messages_dropped =
            m_Stats.total_messages_dropped.load(std::memory_order_relaxed);
        out_stats->total_bytes_published =
            m_Stats.total_bytes_published.load(std::memory_order_relaxed);
        out_stats->total_rpc_calls =
            m_Stats.total_rpc_calls.load(std::memory_order_relaxed);
        out_stats->total_rpc_completions =
            m_Stats.total_rpc_completions.load(std::memory_order_relaxed);
        out_stats->total_rpc_failures =
            m_Stats.total_rpc_failures.load(std::memory_order_relaxed);

        {
            std::lock_guard lock(m_WriteMutex);
            out_stats->active_subscriptions = m_Subscriptions.size();
        }
        out_stats->active_topics = m_TopicRegistry.GetTopicCount();

        {
            std::shared_lock lock(m_RpcMutex);
            out_stats->active_rpc_handlers =
                m_RpcHandlers.size() + m_StreamingRpcHandlers.size();
        }

        out_stats->uptime_ns =
            GetTimestampNs() - m_Stats.start_time.load(std::memory_order_relaxed);
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
        out_info->message_count = m_TopicRegistry.GetMessageCount(topic);

        {
            SnapshotGuard guard(m_Snapshot.load(std::memory_order_acquire));
            if (guard) {
                auto it = guard.get()->topic_subs.find(topic);
                out_info->subscriber_count =
                    (it != guard.get()->topic_subs.end()) ? it->second.subs.size() : 0;
            } else {
                out_info->subscriber_count = 0;
            }
        }

        const std::string *name = m_TopicRegistry.GetName(topic);
        if (name && !name->empty()) {
            size_t copy_len = std::min(name->size(), sizeof(out_info->name) - 1);
            std::memcpy(out_info->name, name->c_str(), copy_len);
            out_info->name[copy_len] = '\0';
        } else {
            out_info->name[0] = '\0';
        }

        return BML_RESULT_OK;
    }

    BML_Result ImcBusImpl::GetTopicName(BML_TopicId topic,
                                        char *buffer,
                                        size_t buffer_size,
                                        size_t *out_length) {
        if (topic == BML_TOPIC_ID_INVALID || !buffer || buffer_size == 0)
            return BML_RESULT_INVALID_ARGUMENT;

        const std::string *name = m_TopicRegistry.GetName(topic);
        if (!name) {
            buffer[0] = '\0';
            if (out_length)
                *out_length = 0;
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
        return PublishState(nullptr, topic, msg);
    }

    BML_Result ImcBusImpl::PublishState(BML_Mod owner, BML_TopicId topic,
                                        const BML_ImcMessage *msg) {
        if (topic == BML_TOPIC_ID_INVALID || !msg)
            return BML_RESULT_INVALID_ARGUMENT;
        if (msg->size > 0 && !msg->data)
            return BML_RESULT_INVALID_ARGUMENT;

        BML_Mod resolved_owner = nullptr;
        BML_CHECK(ResolveRegistrationOwner(owner, &resolved_owner));

        RetainedStateEntry entry;
        entry.owner = resolved_owner;
        entry.timestamp = msg->timestamp ? msg->timestamp : GetTimestampNs();
        entry.flags = msg->flags;
        entry.priority = msg->priority;
        if (!entry.payload.CopyFrom(msg->data, msg->size))
            return BML_RESULT_OUT_OF_MEMORY;

        {
            std::lock_guard<std::mutex> lock(m_StateMutex);
            m_RetainedStates[topic] = std::move(entry);
        }

        BML_ImcMessage publish_msg = *msg;
        publish_msg.timestamp = entry.timestamp;
        return PublishEx(resolved_owner, topic, &publish_msg);
    }

    BML_Result ImcBusImpl::CopyState(BML_TopicId topic,
                                     void *dst,
                                     size_t dst_size,
                                     size_t *out_size,
                                     BML_ImcStateMeta *out_meta) {
        if (topic == BML_TOPIC_ID_INVALID)
            return BML_RESULT_INVALID_ARGUMENT;

        std::lock_guard<std::mutex> lock(m_StateMutex);
        auto it = m_RetainedStates.find(topic);
        if (it == m_RetainedStates.end())
            return BML_RESULT_NOT_FOUND;

        const size_t payload_size = it->second.payload.Size();
        if (out_size)
            *out_size = payload_size;
        if (out_meta) {
            out_meta->struct_size = sizeof(BML_ImcStateMeta);
            out_meta->timestamp = it->second.timestamp;
            out_meta->flags = it->second.flags;
            out_meta->size = payload_size;
        }

        if (payload_size == 0)
            return BML_RESULT_OK;
        if (!dst && dst_size != 0)
            return BML_RESULT_INVALID_ARGUMENT;
        if (!dst || dst_size < payload_size)
            return BML_RESULT_BUFFER_TOO_SMALL;

        std::memcpy(dst, it->second.payload.Data(), payload_size);
        return BML_RESULT_OK;
    }

    BML_Result ImcBusImpl::ClearState(BML_TopicId topic) {
        if (topic == BML_TOPIC_ID_INVALID)
            return BML_RESULT_INVALID_ARGUMENT;

        std::lock_guard<std::mutex> lock(m_StateMutex);
        auto it = m_RetainedStates.find(topic);
        if (it == m_RetainedStates.end())
            return BML_RESULT_NOT_FOUND;
        m_RetainedStates.erase(it);
        return BML_RESULT_OK;
    }
} // namespace BML::Core
