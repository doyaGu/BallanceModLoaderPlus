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

namespace BML::Core {

namespace {

// ============================================================================
// IMC API Implementation Functions
// ============================================================================

BML_Result BML_API_ImcGetTopicId(const char *name, BML_TopicId *out_id) {
    return ImcBus::Instance().GetTopicId(name, out_id);
}

BML_Result BML_API_ImcGetRpcId(const char *name, BML_RpcId *out_id) {
    return ImcBus::Instance().GetRpcId(name, out_id);
}

BML_Result BML_API_ImcPublish(BML_TopicId topic, const void *data, size_t size, const BML_ImcMessage *msg) {
    return ImcBus::Instance().Publish(topic, data, size, msg);
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

BML_Result BML_API_GetImcCaps(BML_ImcCaps *out_caps) {
    if (!out_caps)
        return BML_RESULT_INVALID_ARGUMENT;

    out_caps->struct_size = sizeof(BML_ImcCaps);
    out_caps->api_version = {0, 4, 0};
    out_caps->capability_flags = BML_IMC_CAP_PUBSUB | BML_IMC_CAP_RPC | BML_IMC_CAP_FUTURES |
                                  BML_IMC_CAP_ZERO_COPY | BML_IMC_CAP_PRIORITY | BML_IMC_CAP_STATISTICS;
    out_caps->max_topic_count = 0;  // Unlimited
    out_caps->max_queue_depth = 4096 * 4;  // kMaxQueueCapacity * kPriorityLevels
    out_caps->inline_payload_max = 64;      // kInlinePayloadBytes
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

} // namespace

// ============================================================================
// API Registration
// ============================================================================

void RegisterImcApis() {
    auto &registry = ApiRegistry::Instance();

    // ID Resolution
    BML_REGISTER_API_GUARDED(bmlImcGetTopicId, "imc", BML_API_ImcGetTopicId);
    BML_REGISTER_API_GUARDED(bmlImcGetRpcId, "imc", BML_API_ImcGetRpcId);

    // Pub/Sub
    BML_REGISTER_API_GUARDED(bmlImcPublish, "imc", BML_API_ImcPublish);
    BML_REGISTER_API_GUARDED(bmlImcPublishBuffer, "imc", BML_API_ImcPublishBuffer);
    BML_REGISTER_API_GUARDED(bmlImcPublishMulti, "imc", BML_API_ImcPublishMulti);
    BML_REGISTER_API_GUARDED(bmlImcSubscribe, "imc", BML_API_ImcSubscribe);
    BML_REGISTER_API_GUARDED(bmlImcSubscribeEx, "imc", BML_API_ImcSubscribeEx);
    BML_REGISTER_API_GUARDED(bmlImcUnsubscribe, "imc", BML_API_ImcUnsubscribe);
    BML_REGISTER_API_GUARDED(bmlImcSubscriptionIsActive, "imc", BML_API_ImcSubscriptionIsActive);

    // RPC
    BML_REGISTER_API_GUARDED(bmlImcRegisterRpc, "imc", BML_API_ImcRegisterRpc);
    BML_REGISTER_API_GUARDED(bmlImcUnregisterRpc, "imc", BML_API_ImcUnregisterRpc);
    BML_REGISTER_API_GUARDED(bmlImcCallRpc, "imc", BML_API_ImcCallRpc);

    // Futures
    BML_REGISTER_API_GUARDED(bmlImcFutureAwait, "imc", BML_API_ImcFutureAwait);
    BML_REGISTER_API_GUARDED(bmlImcFutureGetResult, "imc", BML_API_ImcFutureGetResult);
    BML_REGISTER_API_GUARDED(bmlImcFutureGetState, "imc", BML_API_ImcFutureGetState);
    BML_REGISTER_API_GUARDED(bmlImcFutureCancel, "imc", BML_API_ImcFutureCancel);
    BML_REGISTER_API_GUARDED(bmlImcFutureOnComplete, "imc", BML_API_ImcFutureOnComplete);
    BML_REGISTER_API_GUARDED(bmlImcFutureRelease, "imc", BML_API_ImcFutureRelease);

    // Runtime
    BML_REGISTER_API_VOID_GUARDED(bmlImcPump, "imc", BML_API_ImcPump);

    // Diagnostics
    BML_REGISTER_API_GUARDED(bmlGetImcCaps, "imc", BML_API_GetImcCaps);
    BML_REGISTER_API_GUARDED(bmlImcGetSubscriptionStats, "imc", BML_API_ImcGetSubscriptionStats);
    BML_REGISTER_API_GUARDED(bmlImcGetStats, "imc", BML_API_ImcGetStats);
    BML_REGISTER_API_GUARDED(bmlImcResetStats, "imc", BML_API_ImcResetStats);
    BML_REGISTER_API_GUARDED(bmlImcGetTopicInfo, "imc", BML_API_ImcGetTopicInfo);
    BML_REGISTER_API_GUARDED(bmlImcGetTopicName, "imc", BML_API_ImcGetTopicName);
}

} // namespace BML::Core
