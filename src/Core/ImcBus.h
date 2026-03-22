#ifndef BML_CORE_IMC_BUS_H
#define BML_CORE_IMC_BUS_H

#include "bml_imc.h"

namespace BML::Core {

    /// Opaque owner for the internal IMC bus implementation.
    /// Constructed by KernelServices; free functions below access the
    /// impl through the KernelServices-installed instance.
    ///
    /// The destructor is inline (type-erased via function pointer) so that
    /// test targets need not link ImcBus.cpp just to destroy a null
    /// unique_ptr<ImcBus> inside ~KernelServices().
    class ImcBus {
    public:
        ImcBus();

        ~ImcBus() {
            if (m_Deleter)
                m_Deleter(m_Impl);
        }

        ImcBus(const ImcBus &) = delete;
        ImcBus &operator=(const ImcBus &) = delete;

        void BindDeps(class Context &ctx);
        void Shutdown();

    private:
        void *m_Impl = nullptr;
        void (*m_Deleter)(void *) = nullptr;
    };

    // Complete set of free functions wrapping the internal ImcBusImpl.
    // Used by Microkernel, ModuleLifecycle, ModuleRuntime, tests, benchmarks.

    // Lifecycle
    void ImcPump(size_t max_per_sub = 0);
    void ImcShutdown();
    void ImcCleanupOwner(BML_Mod owner);
    void ImcBindDeps(class Context &ctx);

    // ID Resolution
    BML_Result ImcGetTopicId(const char *name, BML_TopicId *out_id);
    BML_Result ImcGetRpcId(const char *name, BML_RpcId *out_id);

    // Pub/Sub
    BML_Result ImcPublish(BML_TopicId topic, const void *data, size_t size);
    BML_Result ImcPublish(BML_Mod owner, BML_TopicId topic, const void *data, size_t size);
    BML_Result ImcPublishEx(BML_TopicId topic, const BML_ImcMessage *msg);
    BML_Result ImcPublishEx(BML_Mod owner, BML_TopicId topic, const BML_ImcMessage *msg);
    BML_Result ImcPublishBuffer(BML_TopicId topic, const BML_ImcBuffer *buffer);
    BML_Result ImcPublishBuffer(BML_Mod owner, BML_TopicId topic,
                                     const BML_ImcBuffer *buffer);
    BML_Result ImcSubscribe(BML_TopicId topic, BML_ImcHandler handler, void *user_data, BML_Subscription *out_sub);
    BML_Result ImcSubscribe(BML_Mod owner, BML_TopicId topic, BML_ImcHandler handler,
                                 void *user_data, BML_Subscription *out_sub);
    BML_Result ImcSubscribeEx(BML_TopicId topic, BML_ImcHandler handler, void *user_data,
                              const BML_SubscribeOptions *options, BML_Subscription *out_sub);
    BML_Result ImcSubscribeEx(BML_Mod owner, BML_TopicId topic, BML_ImcHandler handler,
                                   void *user_data, const BML_SubscribeOptions *options,
                                   BML_Subscription *out_sub);
    BML_Result ImcUnsubscribe(BML_Subscription sub);
    BML_Result ImcSubscriptionIsActive(BML_Subscription sub, BML_Bool *out_active);
    BML_Result ImcGetSubscriptionStats(BML_Subscription sub, BML_SubscriptionStats *out_stats);

    // Intercept
    BML_Result ImcSubscribeIntercept(BML_TopicId topic, BML_ImcInterceptHandler handler,
                                     void *user_data, BML_Subscription *out_sub);
    BML_Result ImcSubscribeIntercept(BML_Mod owner, BML_TopicId topic,
                                          BML_ImcInterceptHandler handler, void *user_data,
                                          BML_Subscription *out_sub);
    BML_Result ImcSubscribeInterceptEx(BML_TopicId topic, BML_ImcInterceptHandler handler,
                                       void *user_data, const BML_SubscribeOptions *options,
                                       BML_Subscription *out_sub);
    BML_Result ImcSubscribeInterceptEx(BML_Mod owner, BML_TopicId topic,
                                            BML_ImcInterceptHandler handler, void *user_data,
                                            const BML_SubscribeOptions *options,
                                            BML_Subscription *out_sub);
    BML_Result ImcPublishInterceptable(BML_TopicId topic, BML_ImcMessage *msg, BML_EventResult *out_result);
    BML_Result ImcPublishInterceptable(BML_Mod owner, BML_TopicId topic,
                                            BML_ImcMessage *msg, BML_EventResult *out_result);
    BML_Result ImcPublishMulti(const BML_TopicId *topics, size_t topic_count,
                               const void *data, size_t size, const BML_ImcMessage *msg,
                               size_t *out_delivered);
    BML_Result ImcPublishMulti(BML_Mod owner, const BML_TopicId *topics, size_t topic_count,
                                    const void *data, size_t size, const BML_ImcMessage *msg,
                                    size_t *out_delivered);

    // RPC
    BML_Result ImcRegisterRpc(BML_RpcId rpc_id, BML_RpcHandler handler, void *user_data);
    BML_Result ImcRegisterRpc(BML_Mod owner, BML_RpcId rpc_id, BML_RpcHandler handler,
                                   void *user_data);
    BML_Result ImcUnregisterRpc(BML_RpcId rpc_id);
    BML_Result ImcUnregisterRpc(BML_Mod owner, BML_RpcId rpc_id);
    BML_Result ImcCallRpc(BML_RpcId rpc_id, const BML_ImcMessage *request, BML_Future *out_future);
    BML_Result ImcCallRpc(BML_Mod owner, BML_RpcId rpc_id,
                               const BML_ImcMessage *request, BML_Future *out_future);

    // RPC v1.1
    BML_Result ImcRegisterRpcEx(BML_RpcId rpc_id, BML_RpcHandlerEx handler, void *user_data);
    BML_Result ImcRegisterRpcEx(BML_Mod owner, BML_RpcId rpc_id, BML_RpcHandlerEx handler,
                                     void *user_data);
    BML_Result ImcCallRpcEx(BML_RpcId rpc_id, const BML_ImcMessage *request, const BML_RpcCallOptions *options, BML_Future *out_future);
    BML_Result ImcCallRpcEx(BML_Mod owner, BML_RpcId rpc_id,
                                 const BML_ImcMessage *request, const BML_RpcCallOptions *options,
                                 BML_Future *out_future);
    BML_Result ImcFutureGetError(BML_Future future, BML_Result *out_code, char *msg, size_t cap, size_t *out_len);
    BML_Result ImcGetRpcInfo(BML_RpcId rpc_id, BML_RpcInfo *out_info);
    BML_Result ImcGetRpcName(BML_RpcId rpc_id, char *buf, size_t cap, size_t *out_len);
    void ImcEnumerateRpc(void(*cb)(BML_RpcId, const char *, BML_Bool, void *), void *user_data);
    BML_Result ImcAddRpcMiddleware(BML_RpcMiddleware middleware, int32_t priority, void *user_data);
    BML_Result ImcAddRpcMiddleware(BML_Mod owner, BML_RpcMiddleware middleware,
                                        int32_t priority, void *user_data);
    BML_Result ImcRemoveRpcMiddleware(BML_RpcMiddleware middleware);
    BML_Result ImcRemoveRpcMiddleware(BML_Mod owner, BML_RpcMiddleware middleware);
    BML_Result ImcRegisterStreamingRpc(BML_RpcId rpc_id, BML_StreamingRpcHandler handler, void *user_data);
    BML_Result ImcRegisterStreamingRpc(BML_Mod owner, BML_RpcId rpc_id,
                                            BML_StreamingRpcHandler handler, void *user_data);
    BML_Result ImcStreamPush(BML_RpcStream stream, const void *data, size_t size);
    BML_Result ImcStreamComplete(BML_RpcStream stream);
    BML_Result ImcStreamError(BML_RpcStream stream, BML_Result error, const char *msg);
    BML_Result ImcCallStreamingRpc(BML_RpcId rpc_id, const BML_ImcMessage *request,
                                    BML_ImcHandler on_chunk, BML_FutureCallback on_done,
                                    void *user_data, BML_Future *out_future);
    BML_Result ImcCallStreamingRpc(BML_Mod owner, BML_RpcId rpc_id,
                                        const BML_ImcMessage *request, BML_ImcHandler on_chunk,
                                        BML_FutureCallback on_done, void *user_data,
                                        BML_Future *out_future);

    // Futures
    BML_Result ImcFutureAwait(BML_Future future, uint32_t timeout_ms);
    BML_Result ImcFutureGetResult(BML_Future future, BML_ImcMessage *out_msg);
    BML_Result ImcFutureGetState(BML_Future future, BML_FutureState *out_state);
    BML_Result ImcFutureCancel(BML_Future future);
    BML_Result ImcFutureOnComplete(BML_Future future, BML_FutureCallback callback, void *user_data);
    BML_Result ImcFutureOnComplete(BML_Mod owner, BML_Future future,
                                        BML_FutureCallback callback, void *user_data);
    BML_Result ImcFutureRelease(BML_Future future);

    // Diagnostics
    BML_Result ImcGetStats(BML_ImcStats *out_stats);
    BML_Result ImcResetStats();
    BML_Result ImcGetTopicInfo(BML_TopicId topic, BML_TopicInfo *out_info);
    BML_Result ImcGetTopicName(BML_TopicId topic, char *buffer, size_t buffer_size, size_t *out_length);

    // State
    BML_Result ImcPublishState(BML_TopicId topic, const BML_ImcMessage *msg);
    BML_Result ImcPublishState(BML_Mod owner, BML_TopicId topic, const BML_ImcMessage *msg);
    BML_Result ImcCopyState(BML_TopicId topic, void *dst, size_t dst_size, size_t *out_size, BML_ImcStateMeta *out_meta);
    BML_Result ImcClearState(BML_TopicId topic);

    void RegisterImcApis();
} // namespace BML::Core

#endif // BML_CORE_IMC_BUS_H
