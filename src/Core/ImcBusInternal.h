#ifndef BML_CORE_IMC_BUS_INTERNAL_H
#define BML_CORE_IMC_BUS_INTERNAL_H

#include "ImcBusPublishInternal.h"
#include "ImcBusRpcInternal.h"
#include "ImcBusStateInternal.h"

#include <thread>

#include "CoreErrors.h"
#include "CrashDumpWriter.h"
#include "FaultTracker.h"
#include "FixedBlockPool.h"
#include "KernelServices.h"

namespace BML::Core {
    ImcBusImpl &GetBus(KernelServices &kernel);
    Context *ContextFromKernel(KernelServices *kernel) noexcept;

    struct TapState {
        BML_ImcMessageTap callback;
        void *user_data;
        BML_Mod owner;
    };

    class ImcBusImpl {
    public:
        ImcBusImpl();

        BML_Result GetTopicId(const char *name, BML_TopicId *out_id);
        BML_Result GetRpcId(const char *name, BML_RpcId *out_id);

        BML_Result Publish(BML_TopicId topic, const void *data, size_t size,
                           uint32_t type_id = BML_PAYLOAD_TYPE_NONE);
        BML_Result Publish(BML_Mod owner, BML_TopicId topic, const void *data, size_t size,
                           uint32_t type_id = BML_PAYLOAD_TYPE_NONE);
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
        void BindDeps(Context &ctx);
        Context *GetContext() const noexcept { return m_Context; }

        BML_Result RegisterMessageTap(BML_Mod owner, BML_ImcMessageTap tap,
                                      void *user_data);
        BML_Result UnregisterMessageTap(BML_Mod owner);

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
        void FireTap(BML_Mod owner, BML_TopicId topic, const BML_ImcMessage *msg);

        PublishRuntimeState m_PublishState;
        RpcRuntimeState m_RpcState;
        RetainedStateStore m_RetainedStateStore;
        ImcGlobalStats m_GlobalStats;
        std::atomic<uint64_t> m_NextMessageId{1};
        Context *m_Context{nullptr};
        std::atomic<TapState *> m_Tap{nullptr};
    };
} // namespace BML::Core

#endif // BML_CORE_IMC_BUS_INTERNAL_H
