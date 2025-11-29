/**
 * @file ImcApi.cpp
 * @brief IMC (Inter-Module Communication) API registration
 *
 * Registers all IMC-related APIs with the ApiRegistry using standardized macros.
 */

#include "ApiRegistry.h"
#include "ApiRegistrationMacros.h"
#include "ImcBus.h"
#include "bml_imc.h"
#include "bml_api_ids.h"
#include "bml_capabilities.h"

namespace BML::Core {
    BML_Result BML_API_ImcGetTopicId(const char *name, BML_TopicId *out_id) {
        return ImcBus::Instance().GetTopicId(name, out_id);
    }

    BML_Result BML_API_ImcGetRpcId(const char *name, BML_RpcId *out_id) {
        return ImcBus::Instance().GetRpcId(name, out_id);
    }

    BML_Result BML_API_ImcPublish(BML_TopicId topic, const void *data, size_t size) {
        return ImcBus::Instance().Publish(topic, data, size);
    }

    BML_Result BML_API_ImcPublishEx(BML_TopicId topic, const BML_ImcMessage *msg) {
        return ImcBus::Instance().PublishEx(topic, msg);
    }

    BML_Result BML_API_ImcPublishBuffer(BML_TopicId topic, const BML_ImcBuffer *buffer) {
        return ImcBus::Instance().PublishBuffer(topic, buffer);
    }

    BML_Result BML_API_ImcPublishMulti(const BML_TopicId *topics, size_t topic_count,
                                       const void *data, size_t size, const BML_ImcMessage *msg,
                                       size_t *out_delivered) {
        return ImcBus::Instance().PublishMulti(topics, topic_count, data, size, msg, out_delivered);
    }

    BML_Result BML_API_ImcSubscribe(BML_TopicId topic, BML_ImcHandler handler, void *user_data, BML_Subscription *out_sub) {
        return ImcBus::Instance().Subscribe(topic, handler, user_data, out_sub);
    }

    BML_Result BML_API_ImcSubscribeEx(BML_TopicId topic, BML_ImcHandler handler, void *user_data,
                                      const BML_SubscribeOptions *options, BML_Subscription *out_sub) {
        return ImcBus::Instance().SubscribeEx(topic, handler, user_data, options, out_sub);
    }

    BML_Result BML_API_ImcUnsubscribe(BML_Subscription sub) {
        return ImcBus::Instance().Unsubscribe(sub);
    }

    BML_Result BML_API_ImcSubscriptionIsActive(BML_Subscription sub, BML_Bool *out_active) {
        return ImcBus::Instance().SubscriptionIsActive(sub, out_active);
    }

    BML_Result BML_API_ImcRegisterRpc(BML_RpcId rpc_id, BML_RpcHandler handler, void *user_data) {
        return ImcBus::Instance().RegisterRpc(rpc_id, handler, user_data);
    }

    BML_Result BML_API_ImcUnregisterRpc(BML_RpcId rpc_id) {
        return ImcBus::Instance().UnregisterRpc(rpc_id);
    }

    BML_Result BML_API_ImcCallRpc(BML_RpcId rpc_id, const BML_ImcMessage *request, BML_Future *out_future) {
        return ImcBus::Instance().CallRpc(rpc_id, request, out_future);
    }

    BML_Result BML_API_ImcFutureAwait(BML_Future future, uint32_t timeout_ms) {
        return ImcBus::Instance().FutureAwait(future, timeout_ms);
    }

    BML_Result BML_API_ImcFutureGetResult(BML_Future future, BML_ImcMessage *out_msg) {
        return ImcBus::Instance().FutureGetResult(future, out_msg);
    }

    BML_Result BML_API_ImcFutureGetState(BML_Future future, BML_FutureState *out_state) {
        return ImcBus::Instance().FutureGetState(future, out_state);
    }

    BML_Result BML_API_ImcFutureCancel(BML_Future future) {
        return ImcBus::Instance().FutureCancel(future);
    }

    BML_Result BML_API_ImcFutureOnComplete(BML_Future future, BML_FutureCallback callback, void *user_data) {
        return ImcBus::Instance().FutureOnComplete(future, callback, user_data);
    }

    BML_Result BML_API_ImcFutureRelease(BML_Future future) {
        return ImcBus::Instance().FutureRelease(future);
    }

    void BML_API_ImcPump(size_t max_per_sub) {
        ImcBus::Instance().Pump(max_per_sub);
    }

    BML_Result BML_API_ImcGetCaps(BML_ImcCaps *out_caps) {
        if (!out_caps)
            return BML_RESULT_INVALID_ARGUMENT;

        out_caps->struct_size = sizeof(BML_ImcCaps);
        out_caps->api_version = bmlGetApiVersion();
        out_caps->capability_flags = BML_IMC_CAP_PUBSUB | BML_IMC_CAP_RPC | BML_IMC_CAP_FUTURES |
            BML_IMC_CAP_ZERO_COPY | BML_IMC_CAP_PRIORITY | BML_IMC_CAP_STATISTICS;
        out_caps->max_topic_count = 0;        // Unlimited
        out_caps->max_queue_depth = 4096 * 4; // kMaxQueueCapacity * kPriorityLevels
        out_caps->inline_payload_max = 64;    // kInlinePayloadBytes
        return BML_RESULT_OK;
    }

    BML_Result BML_API_ImcGetSubscriptionStats(BML_Subscription sub, BML_SubscriptionStats *out_stats) {
        return ImcBus::Instance().GetSubscriptionStats(sub, out_stats);
    }

    BML_Result BML_API_ImcGetStats(BML_ImcStats *out_stats) {
        return ImcBus::Instance().GetStats(out_stats);
    }

    BML_Result BML_API_ImcResetStats() {
        return ImcBus::Instance().ResetStats();
    }

    BML_Result BML_API_ImcGetTopicInfo(BML_TopicId topic, BML_TopicInfo *out_info) {
        return ImcBus::Instance().GetTopicInfo(topic, out_info);
    }

    BML_Result BML_API_ImcGetTopicName(BML_TopicId topic, char *buffer, size_t buffer_size, size_t *out_length) {
        return ImcBus::Instance().GetTopicName(topic, buffer, buffer_size, out_length);
    }

    void RegisterImcApis() {
        BML_BEGIN_API_REGISTRATION();

        // ID Resolution
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlImcGetTopicId, "imc", BML_API_ImcGetTopicId, BML_CAP_IMC_BASIC | BML_CAP_IMC_ID_BASED);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlImcGetRpcId, "imc", BML_API_ImcGetRpcId, BML_CAP_IMC_RPC | BML_CAP_IMC_ID_BASED);

        // Pub/Sub
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlImcPublish, "imc", BML_API_ImcPublish, BML_CAP_IMC_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlImcPublishEx, "imc", BML_API_ImcPublishEx, BML_CAP_IMC_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlImcPublishBuffer, "imc", BML_API_ImcPublishBuffer, BML_CAP_IMC_BASIC | BML_CAP_IMC_BUFFER);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlImcPublishMulti, "imc", BML_API_ImcPublishMulti, BML_CAP_IMC_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlImcSubscribe, "imc", BML_API_ImcSubscribe, BML_CAP_IMC_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlImcSubscribeEx, "imc", BML_API_ImcSubscribeEx, BML_CAP_IMC_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlImcUnsubscribe, "imc", BML_API_ImcUnsubscribe, BML_CAP_IMC_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlImcSubscriptionIsActive, "imc", BML_API_ImcSubscriptionIsActive, BML_CAP_IMC_BASIC);

        // RPC
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlImcRegisterRpc, "imc", BML_API_ImcRegisterRpc, BML_CAP_IMC_RPC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlImcUnregisterRpc, "imc", BML_API_ImcUnregisterRpc, BML_CAP_IMC_RPC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlImcCallRpc, "imc", BML_API_ImcCallRpc, BML_CAP_IMC_RPC);

        // Futures
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlImcFutureAwait, "imc", BML_API_ImcFutureAwait, BML_CAP_IMC_FUTURE);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlImcFutureGetResult, "imc", BML_API_ImcFutureGetResult, BML_CAP_IMC_FUTURE);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlImcFutureGetState, "imc", BML_API_ImcFutureGetState, BML_CAP_IMC_FUTURE);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlImcFutureCancel, "imc", BML_API_ImcFutureCancel, BML_CAP_IMC_FUTURE);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlImcFutureOnComplete, "imc", BML_API_ImcFutureOnComplete, BML_CAP_IMC_FUTURE);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlImcFutureRelease, "imc", BML_API_ImcFutureRelease, BML_CAP_IMC_FUTURE);

        // Runtime
        BML_REGISTER_API_VOID_GUARDED_WITH_CAPS(bmlImcPump, "imc", BML_API_ImcPump, BML_CAP_IMC_DISPATCH);

        // Diagnostics
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlImcGetCaps, "imc", BML_API_ImcGetCaps, BML_CAP_IMC_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlImcGetSubscriptionStats, "imc", BML_API_ImcGetSubscriptionStats, BML_CAP_IMC_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlImcGetStats, "imc", BML_API_ImcGetStats, BML_CAP_IMC_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlImcResetStats, "imc", BML_API_ImcResetStats, BML_CAP_IMC_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlImcGetTopicInfo, "imc", BML_API_ImcGetTopicInfo, BML_CAP_IMC_BASIC);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlImcGetTopicName, "imc", BML_API_ImcGetTopicName, BML_CAP_IMC_BASIC);
    }
} // namespace BML::Core
