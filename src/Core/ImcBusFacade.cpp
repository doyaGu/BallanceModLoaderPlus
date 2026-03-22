#include "ImcBusInternal.h"

namespace BML::Core {
    void ImcPump(size_t max_per_sub) { GetBus().Pump(max_per_sub); }
    void ImcShutdown() { GetBus().Shutdown(); }
    void ImcCleanupOwner(BML_Mod owner) { GetBus().CleanupOwner(owner); }
    void ImcBindDeps(Context &ctx) {
        g_BusContext = &ctx;
    }

    BML_Result ImcGetTopicId(const char *n, BML_TopicId *o) { return GetBus().GetTopicId(n, o); }
    BML_Result ImcGetRpcId(const char *n, BML_RpcId *o) { return GetBus().GetRpcId(n, o); }
    BML_Result ImcPublish(BML_TopicId t, const void *d, size_t s) { return GetBus().Publish(t, d, s); }
    BML_Result ImcPublish(BML_Mod owner, BML_TopicId t, const void *d,
                          size_t s) { return GetBus().Publish(owner, t, d, s); }
    BML_Result ImcPublishEx(BML_TopicId t, const BML_ImcMessage *m) { return GetBus().PublishEx(t, m); }
    BML_Result ImcPublishEx(BML_Mod owner, BML_TopicId t,
                            const BML_ImcMessage *m) { return GetBus().PublishEx(owner, t, m); }
    BML_Result ImcPublishBuffer(BML_TopicId t, const BML_ImcBuffer *b) { return GetBus().PublishBuffer(t, b); }
    BML_Result ImcPublishBuffer(BML_Mod owner, BML_TopicId t,
                                const BML_ImcBuffer *b) { return GetBus().PublishBuffer(owner, t, b); }
    BML_Result ImcSubscribe(BML_TopicId t, BML_ImcHandler h, void *u,
                            BML_Subscription *o) { return GetBus().Subscribe(t, h, u, o); }
    BML_Result ImcSubscribe(BML_Mod owner, BML_TopicId t, BML_ImcHandler h,
                            void *u, BML_Subscription *o) { return GetBus().Subscribe(owner, t, h, u, o); }
    BML_Result ImcSubscribeEx(BML_TopicId t, BML_ImcHandler h, void *u,
                              const BML_SubscribeOptions *opts,
                              BML_Subscription *o) { return GetBus().SubscribeEx(t, h, u, opts, o); }
    BML_Result ImcSubscribeEx(BML_Mod owner, BML_TopicId t, BML_ImcHandler h,
                              void *u, const BML_SubscribeOptions *opts,
                              BML_Subscription *o) {
        return GetBus().SubscribeEx(owner, t, h, u, opts, o);
    }
    BML_Result ImcUnsubscribe(BML_Subscription s) { return GetBus().Unsubscribe(s); }
    BML_Result ImcSubscriptionIsActive(BML_Subscription s, BML_Bool *o) {
        return GetBus().SubscriptionIsActive(s, o);
    }
    BML_Result ImcGetSubscriptionStats(BML_Subscription s, BML_SubscriptionStats *o) {
        return GetBus().GetSubscriptionStats(s, o);
    }
    BML_Result ImcSubscribeIntercept(BML_TopicId t, BML_ImcInterceptHandler h,
                                     void *u, BML_Subscription *o) {
        return GetBus().SubscribeIntercept(t, h, u, o);
    }
    BML_Result ImcSubscribeIntercept(BML_Mod owner, BML_TopicId t,
                                     BML_ImcInterceptHandler h, void *u,
                                     BML_Subscription *o) {
        return GetBus().SubscribeIntercept(owner, t, h, u, o);
    }
    BML_Result ImcSubscribeInterceptEx(BML_TopicId t, BML_ImcInterceptHandler h,
                                       void *u, const BML_SubscribeOptions *opts,
                                       BML_Subscription *o) {
        return GetBus().SubscribeInterceptEx(t, h, u, opts, o);
    }
    BML_Result ImcSubscribeInterceptEx(BML_Mod owner, BML_TopicId t,
                                       BML_ImcInterceptHandler h, void *u,
                                       const BML_SubscribeOptions *opts,
                                       BML_Subscription *o) {
        return GetBus().SubscribeInterceptEx(owner, t, h, u, opts, o);
    }
    BML_Result ImcPublishInterceptable(BML_TopicId t, BML_ImcMessage *m, BML_EventResult *o) {
        return GetBus().PublishInterceptable(t, m, o);
    }
    BML_Result ImcPublishInterceptable(BML_Mod owner, BML_TopicId t, BML_ImcMessage *m,
                                       BML_EventResult *o) {
        return GetBus().PublishInterceptable(owner, t, m, o);
    }
    BML_Result ImcPublishMulti(const BML_TopicId *ts, size_t n, const void *d, size_t s,
                               const BML_ImcMessage *m, size_t *o) {
        return GetBus().PublishMulti(ts, n, d, s, m, o);
    }
    BML_Result ImcPublishMulti(BML_Mod owner, const BML_TopicId *ts, size_t n,
                               const void *d, size_t s, const BML_ImcMessage *m,
                               size_t *o) {
        return GetBus().PublishMulti(owner, ts, n, d, s, m, o);
    }
    BML_Result ImcRegisterRpc(BML_RpcId r, BML_RpcHandler h, void *u) {
        return GetBus().RegisterRpc(r, h, u);
    }
    BML_Result ImcRegisterRpc(BML_Mod owner, BML_RpcId r, BML_RpcHandler h,
                              void *u) { return GetBus().RegisterRpc(owner, r, h, u); }
    BML_Result ImcUnregisterRpc(BML_RpcId r) { return GetBus().UnregisterRpc(r); }
    BML_Result ImcUnregisterRpc(BML_Mod owner, BML_RpcId r) {
        return GetBus().UnregisterRpc(owner, r);
    }
    BML_Result ImcCallRpc(BML_RpcId r, const BML_ImcMessage *req, BML_Future *o) {
        return GetBus().CallRpc(r, req, o);
    }
    BML_Result ImcCallRpc(BML_Mod owner, BML_RpcId r, const BML_ImcMessage *req,
                          BML_Future *o) { return GetBus().CallRpc(owner, r, req, o); }
    BML_Result ImcFutureAwait(BML_Future f, uint32_t t) { return GetBus().FutureAwait(f, t); }
    BML_Result ImcFutureGetResult(BML_Future f, BML_ImcMessage *o) { return GetBus().FutureGetResult(f, o); }
    BML_Result ImcFutureGetState(BML_Future f, BML_FutureState *o) { return GetBus().FutureGetState(f, o); }
    BML_Result ImcFutureCancel(BML_Future f) { return GetBus().FutureCancel(f); }
    BML_Result ImcFutureOnComplete(BML_Future f, BML_FutureCallback cb, void *u) {
        return GetBus().FutureOnComplete(f, cb, u);
    }
    BML_Result ImcFutureOnComplete(BML_Mod owner, BML_Future f,
                                   BML_FutureCallback cb, void *u) {
        return GetBus().FutureOnComplete(owner, f, cb, u);
    }
    BML_Result ImcFutureRelease(BML_Future f) { return GetBus().FutureRelease(f); }
    BML_Result ImcGetStats(BML_ImcStats *o) { return GetBus().GetStats(o); }
    BML_Result ImcResetStats() { return GetBus().ResetStats(); }
    BML_Result ImcGetTopicInfo(BML_TopicId t, BML_TopicInfo *o) { return GetBus().GetTopicInfo(t, o); }
    BML_Result ImcGetTopicName(BML_TopicId t, char *b, size_t s, size_t *o) {
        return GetBus().GetTopicName(t, b, s, o);
    }
    BML_Result ImcPublishState(BML_TopicId t, const BML_ImcMessage *m) { return GetBus().PublishState(t, m); }
    BML_Result ImcPublishState(BML_Mod owner, BML_TopicId t,
                               const BML_ImcMessage *m) { return GetBus().PublishState(owner, t, m); }
    BML_Result ImcCopyState(BML_TopicId t, void *d, size_t ds, size_t *os,
                            BML_ImcStateMeta *om) {
        return GetBus().CopyState(t, d, ds, os, om);
    }
    BML_Result ImcClearState(BML_TopicId t) { return GetBus().ClearState(t); }

    BML_Result ImcRegisterRpcEx(BML_RpcId r, BML_RpcHandlerEx h, void *u) {
        return GetBus().RegisterRpcEx(r, h, u);
    }
    BML_Result ImcRegisterRpcEx(BML_Mod owner, BML_RpcId r, BML_RpcHandlerEx h,
                                void *u) { return GetBus().RegisterRpcEx(owner, r, h, u); }
    BML_Result ImcCallRpcEx(BML_RpcId r, const BML_ImcMessage *req,
                            const BML_RpcCallOptions *opts, BML_Future *o) {
        return GetBus().CallRpcEx(r, req, opts, o);
    }
    BML_Result ImcCallRpcEx(BML_Mod owner, BML_RpcId r, const BML_ImcMessage *req,
                            const BML_RpcCallOptions *opts, BML_Future *o) {
        return GetBus().CallRpcEx(owner, r, req, opts, o);
    }
    BML_Result ImcFutureGetError(BML_Future f, BML_Result *c, char *m, size_t cap, size_t *ol) {
        return GetBus().FutureGetError(f, c, m, cap, ol);
    }
    BML_Result ImcGetRpcInfo(BML_RpcId r, BML_RpcInfo *o) { return GetBus().GetRpcInfo(r, o); }
    BML_Result ImcGetRpcName(BML_RpcId r, char *b, size_t c, size_t *o) {
        return GetBus().GetRpcName(r, b, c, o);
    }
    void ImcEnumerateRpc(void(*cb)(BML_RpcId, const char *, BML_Bool, void *), void *u) {
        GetBus().EnumerateRpc(cb, u);
    }
    BML_Result ImcAddRpcMiddleware(BML_RpcMiddleware mw, int32_t p, void *u) {
        return GetBus().AddRpcMiddleware(mw, p, u);
    }
    BML_Result ImcAddRpcMiddleware(BML_Mod owner, BML_RpcMiddleware mw,
                                   int32_t p, void *u) {
        return GetBus().AddRpcMiddleware(owner, mw, p, u);
    }
    BML_Result ImcRemoveRpcMiddleware(BML_RpcMiddleware mw) {
        return GetBus().RemoveRpcMiddleware(mw);
    }
    BML_Result ImcRemoveRpcMiddleware(BML_Mod owner, BML_RpcMiddleware mw) {
        return GetBus().RemoveRpcMiddleware(owner, mw);
    }
    BML_Result ImcRegisterStreamingRpc(BML_RpcId r, BML_StreamingRpcHandler h, void *u) {
        return GetBus().RegisterStreamingRpc(r, h, u);
    }
    BML_Result ImcRegisterStreamingRpc(BML_Mod owner, BML_RpcId r,
                                       BML_StreamingRpcHandler h, void *u) {
        return GetBus().RegisterStreamingRpc(owner, r, h, u);
    }
    BML_Result ImcStreamPush(BML_RpcStream s, const void *d, size_t sz) {
        return GetBus().StreamPush(s, d, sz);
    }
    BML_Result ImcStreamComplete(BML_RpcStream s) { return GetBus().StreamComplete(s); }
    BML_Result ImcStreamError(BML_RpcStream s, BML_Result e, const char *m) {
        return GetBus().StreamError(s, e, m);
    }
    BML_Result ImcCallStreamingRpc(BML_RpcId r, const BML_ImcMessage *req,
                                   BML_ImcHandler oc, BML_FutureCallback od,
                                   void *u, BML_Future *o) {
        return GetBus().CallStreamingRpc(r, req, oc, od, u, o);
    }
    BML_Result ImcCallStreamingRpc(BML_Mod owner, BML_RpcId r,
                                   const BML_ImcMessage *req, BML_ImcHandler oc,
                                   BML_FutureCallback od, void *u,
                                   BML_Future *o) {
        return GetBus().CallStreamingRpc(owner, r, req, oc, od, u, o);
    }
} // namespace BML::Core
