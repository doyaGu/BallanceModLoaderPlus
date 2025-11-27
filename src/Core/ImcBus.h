#ifndef BML_CORE_IMC_BUS_H
#define BML_CORE_IMC_BUS_H

#include "bml_imc.h"

namespace BML::Core {
    /**
     * @brief Robust, high-performance IMC Bus implementation
     *
     * Features:
     * - Lock-free MPSC queues for high throughput
     * - Priority message queues
     * - Zero-copy buffer support
     * - Per-subscription message filtering
     * - Configurable backpressure policies
     * - Comprehensive statistics and diagnostics
     * - Memory pool for allocation-free hot paths
     *
     * API Count: 25
     * - ID Resolution: 2 (GetTopicId, GetRpcId)
     * - Pub/Sub: 8 (Publish, PublishBuffer, PublishMulti, Subscribe, SubscribeEx, Unsubscribe, IsActive, GetStats)
     * - RPC: 3 (RegisterRpc, UnregisterRpc, CallRpc)
     * - Futures: 6 (Await, GetResult, GetState, Cancel, OnComplete, Release)
     * - Diagnostics: 5 (GetStats, ResetStats, GetTopicInfo, GetTopicName, GetCaps)
     * - Pump: 1
     */
    class ImcBus {
    public:
        static ImcBus &Instance();

        // ID Resolution
        BML_Result GetTopicId(const char *name, BML_TopicId *out_id);
        BML_Result GetRpcId(const char *name, BML_RpcId *out_id);

        // Pub/Sub
        BML_Result Publish(BML_TopicId topic,
                           const void *data,
                           size_t size,
                           const BML_ImcMessage *msg);

        BML_Result PublishBuffer(BML_TopicId topic,
                                 const BML_ImcBuffer *buffer);

        BML_Result PublishMulti(const BML_TopicId *topics,
                                size_t count,
                                const void *data,
                                size_t size,
                                const BML_ImcMessage *msg,
                                size_t *out_delivered);

        BML_Result Subscribe(BML_TopicId topic,
                             BML_ImcHandler handler,
                             void *user_data,
                             BML_Subscription *out_sub);

        BML_Result SubscribeEx(BML_TopicId topic,
                               BML_ImcHandler handler,
                               void *user_data,
                               const BML_SubscribeOptions *options,
                               BML_Subscription *out_sub);

        BML_Result Unsubscribe(BML_Subscription sub);

        BML_Result SubscriptionIsActive(BML_Subscription sub, BML_Bool *out_active);

        BML_Result GetSubscriptionStats(BML_Subscription sub, BML_SubscriptionStats *stats);

        // RPC
        BML_Result RegisterRpc(BML_RpcId rpc_id, BML_RpcHandler handler, void *user_data);
        BML_Result UnregisterRpc(BML_RpcId rpc_id);
        BML_Result CallRpc(BML_RpcId rpc_id,
                           const BML_ImcMessage *request,
                           BML_Future *out_future);

        // Futures
        BML_Result FutureAwait(BML_Future future, uint32_t timeout_ms);
        BML_Result FutureGetResult(BML_Future future, BML_ImcMessage *out_message);
        BML_Result FutureGetState(BML_Future future, BML_FutureState *out_state);
        BML_Result FutureCancel(BML_Future future);
        BML_Result FutureOnComplete(BML_Future future,
                                    BML_FutureCallback callback,
                                    void *user_data);
        BML_Result FutureRelease(BML_Future future);

        // Diagnostics
        BML_Result GetStats(BML_ImcStats *stats);
        BML_Result ResetStats();
        BML_Result GetTopicInfo(BML_TopicId topic_id, BML_TopicInfo *info);
        BML_Result GetTopicName(BML_TopicId topic_id, char *buffer, size_t size, size_t *out_len);

        // Pump
        void Pump(size_t max_per_sub = 0);
        void Shutdown();

    private:
        ImcBus();
    };

    void RegisterImcApis();
} // namespace BML::Core

#endif // BML_CORE_IMC_BUS_H
