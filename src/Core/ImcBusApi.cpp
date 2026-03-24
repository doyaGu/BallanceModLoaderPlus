#include "ImcBus.h"

#include "ApiRegistrationMacros.h"

namespace BML::Core {
    namespace {
        BML_Result BML_API_ImcPublish(BML_Mod owner, BML_TopicId t,
                                      const void *d, size_t s) {
            return ImcPublish(owner, t, d, s);
        }
        BML_Result BML_API_ImcPublishEx(BML_Mod owner, BML_TopicId t,
                                        const BML_ImcMessage *m) {
            return ImcPublishEx(owner, t, m);
        }
        BML_Result BML_API_ImcPublishBuffer(BML_Mod owner, BML_TopicId t,
                                            const BML_ImcBuffer *b) {
            return ImcPublishBuffer(owner, t, b);
        }
        BML_Result BML_API_ImcPublishMulti(BML_Mod owner, const BML_TopicId *ts,
                                           size_t n, const void *d, size_t s,
                                           const BML_ImcMessage *m, size_t *o) {
            return ImcPublishMulti(owner, ts, n, d, s, m, o);
        }
        BML_Result BML_API_ImcSubscribe(BML_Mod owner, BML_TopicId t, BML_ImcHandler h,
                                        void *u, BML_Subscription *o) {
            return ImcSubscribe(owner, t, h, u, o);
        }
        BML_Result BML_API_ImcSubscribeEx(BML_Mod owner, BML_TopicId t, BML_ImcHandler h,
                                          void *u, const BML_SubscribeOptions *opts,
                                          BML_Subscription *o) {
            return ImcSubscribeEx(owner, t, h, u, opts, o);
        }
        BML_Result BML_API_ImcUnsubscribe(BML_Subscription s) { return ImcUnsubscribe(s); }
        BML_Result BML_API_ImcSubscriptionIsActive(BML_Subscription s, BML_Bool *o) {
            return ImcSubscriptionIsActive(s, o);
        }
        BML_Result BML_API_ImcSubscribeIntercept(BML_Mod owner, BML_TopicId t,
                                                 BML_ImcInterceptHandler h, void *u,
                                                 BML_Subscription *o) {
            return ImcSubscribeIntercept(owner, t, h, u, o);
        }
        BML_Result BML_API_ImcSubscribeInterceptEx(BML_Mod owner, BML_TopicId t,
                                                   BML_ImcInterceptHandler h, void *u,
                                                   const BML_SubscribeOptions *opts,
                                                   BML_Subscription *o) {
            return ImcSubscribeInterceptEx(owner, t, h, u, opts, o);
        }
        BML_Result BML_API_ImcPublishInterceptable(BML_Mod owner, BML_TopicId t,
                                                   BML_ImcMessage *m, BML_EventResult *o) {
            return ImcPublishInterceptable(owner, t, m, o);
        }
        BML_Result BML_API_ImcRegisterRpc(BML_Mod owner, BML_RpcId r, BML_RpcHandler h,
                                          void *u) {
            return ImcRegisterRpc(owner, r, h, u);
        }
        BML_Result BML_API_ImcUnregisterRpc(BML_Mod owner, BML_RpcId r) {
            return ImcUnregisterRpc(owner, r);
        }
        BML_Result BML_API_ImcCallRpc(BML_Mod owner, BML_RpcId r,
                                      const BML_ImcMessage *req, BML_Future *o) {
            return ImcCallRpc(owner, r, req, o);
        }
        BML_Result BML_API_ImcFutureAwait(BML_Future f, uint32_t t) { return ImcFutureAwait(f, t); }
        BML_Result BML_API_ImcFutureGetResult(BML_Future f, BML_ImcMessage *o) {
            return ImcFutureGetResult(f, o);
        }
        BML_Result BML_API_ImcFutureGetState(BML_Future f, BML_FutureState *o) {
            return ImcFutureGetState(f, o);
        }
        BML_Result BML_API_ImcFutureCancel(BML_Future f) { return ImcFutureCancel(f); }
        BML_Result BML_API_ImcFutureOnComplete(BML_Mod owner, BML_Future f,
                                               BML_FutureCallback cb, void *u) {
            return ImcFutureOnComplete(owner, f, cb, u);
        }
        BML_Result BML_API_ImcFutureRelease(BML_Future f) { return ImcFutureRelease(f); }
        BML_Result BML_API_ImcGetSubscriptionStats(BML_Subscription s, BML_SubscriptionStats *o) {
            return ImcGetSubscriptionStats(s, o);
        }
        BML_Result BML_API_ImcPublishState(BML_Mod owner, BML_TopicId t,
                                           const BML_ImcMessage *m) {
            return ImcPublishState(owner, t, m);
        }
        BML_Result BML_API_ImcRegisterRpcEx(BML_Mod owner, BML_RpcId r,
                                            BML_RpcHandlerEx h, void *u) {
            return ImcRegisterRpcEx(owner, r, h, u);
        }
        BML_Result BML_API_ImcCallRpcEx(BML_Mod owner, BML_RpcId r,
                                        const BML_ImcMessage *req,
                                        const BML_RpcCallOptions *opts, BML_Future *o) {
            return ImcCallRpcEx(owner, r, req, opts, o);
        }
        BML_Result BML_API_ImcFutureGetError(BML_Future f, BML_Result *c, char *m, size_t cap,
                                             size_t *ol) {
            return ImcFutureGetError(f, c, m, cap, ol);
        }
        BML_Result BML_API_ImcAddRpcMiddleware(BML_Mod owner, BML_RpcMiddleware mw,
                                               int32_t p, void *u) {
            return ImcAddRpcMiddleware(owner, mw, p, u);
        }
        BML_Result BML_API_ImcRemoveRpcMiddleware(BML_Mod owner, BML_RpcMiddleware mw) {
            return ImcRemoveRpcMiddleware(owner, mw);
        }
        BML_Result BML_API_ImcRegisterStreamingRpc(BML_Mod owner, BML_RpcId r,
                                                   BML_StreamingRpcHandler h, void *u) {
            return ImcRegisterStreamingRpc(owner, r, h, u);
        }
        BML_Result BML_API_ImcStreamPush(BML_RpcStream s, const void *d, size_t sz) {
            return ImcStreamPush(s, d, sz);
        }
        BML_Result BML_API_ImcStreamComplete(BML_RpcStream s) { return ImcStreamComplete(s); }
        BML_Result BML_API_ImcStreamError(BML_RpcStream s, BML_Result e, const char *m) {
            return ImcStreamError(s, e, m);
        }
        BML_Result BML_API_ImcCallStreamingRpc(BML_Mod owner, BML_RpcId r,
                                               const BML_ImcMessage *req,
                                               BML_ImcHandler oc, BML_FutureCallback od,
                                               void *u, BML_Future *o) {
            return ImcCallStreamingRpc(owner, r, req, oc, od, u, o);
        }
    } // namespace

    void RegisterImcApis(ApiRegistry &apiRegistry) {
        BML_BEGIN_API_REGISTRATION(apiRegistry);

        BML_REGISTER_API_GUARDED(bmlImcPublish, "imc", BML_API_ImcPublish);
        BML_REGISTER_API_GUARDED(bmlImcPublishEx, "imc", BML_API_ImcPublishEx);
        BML_REGISTER_API_GUARDED(bmlImcPublishBuffer, "imc", BML_API_ImcPublishBuffer);
        BML_REGISTER_API_GUARDED(bmlImcPublishMulti, "imc", BML_API_ImcPublishMulti);
        BML_REGISTER_API_GUARDED(bmlImcSubscribe, "imc", BML_API_ImcSubscribe);
        BML_REGISTER_API_GUARDED(bmlImcSubscribeEx, "imc", BML_API_ImcSubscribeEx);
        BML_REGISTER_API_GUARDED(bmlImcUnsubscribe, "imc", BML_API_ImcUnsubscribe);
        BML_REGISTER_API_GUARDED(
            bmlImcSubscriptionIsActive, "imc", BML_API_ImcSubscriptionIsActive);
        BML_REGISTER_API_GUARDED(
            bmlImcSubscribeIntercept, "imc", BML_API_ImcSubscribeIntercept);
        BML_REGISTER_API_GUARDED(
            bmlImcSubscribeInterceptEx, "imc", BML_API_ImcSubscribeInterceptEx);
        BML_REGISTER_API_GUARDED(
            bmlImcPublishInterceptable, "imc", BML_API_ImcPublishInterceptable);
        BML_REGISTER_API_GUARDED(bmlImcRegisterRpc, "imc", BML_API_ImcRegisterRpc);
        BML_REGISTER_API_GUARDED(bmlImcUnregisterRpc, "imc", BML_API_ImcUnregisterRpc);
        BML_REGISTER_API_GUARDED(bmlImcCallRpc, "imc", BML_API_ImcCallRpc);
        BML_REGISTER_API_GUARDED(bmlImcFutureAwait, "imc", BML_API_ImcFutureAwait);
        BML_REGISTER_API_GUARDED(
            bmlImcFutureGetResult, "imc", BML_API_ImcFutureGetResult);
        BML_REGISTER_API_GUARDED(
            bmlImcFutureGetState, "imc", BML_API_ImcFutureGetState);
        BML_REGISTER_API_GUARDED(bmlImcFutureCancel, "imc", BML_API_ImcFutureCancel);
        BML_REGISTER_API_GUARDED(
            bmlImcFutureOnComplete, "imc", BML_API_ImcFutureOnComplete);
        BML_REGISTER_API_GUARDED(bmlImcFutureRelease, "imc", BML_API_ImcFutureRelease);
        BML_REGISTER_API_GUARDED(
            bmlImcGetSubscriptionStats, "imc", BML_API_ImcGetSubscriptionStats);
        BML_REGISTER_API_GUARDED(bmlImcPublishState, "imc", BML_API_ImcPublishState);
        BML_REGISTER_API_GUARDED(bmlImcRegisterRpcEx, "imc", BML_API_ImcRegisterRpcEx);
        BML_REGISTER_API_GUARDED(bmlImcCallRpcEx, "imc", BML_API_ImcCallRpcEx);
        BML_REGISTER_API_GUARDED(
            bmlImcFutureGetError, "imc", BML_API_ImcFutureGetError);
        BML_REGISTER_API_GUARDED(
            bmlImcAddRpcMiddleware, "imc", BML_API_ImcAddRpcMiddleware);
        BML_REGISTER_API_GUARDED(
            bmlImcRemoveRpcMiddleware, "imc", BML_API_ImcRemoveRpcMiddleware);
        BML_REGISTER_API_GUARDED(
            bmlImcRegisterStreamingRpc, "imc", BML_API_ImcRegisterStreamingRpc);
        BML_REGISTER_API_GUARDED(bmlImcStreamPush, "imc", BML_API_ImcStreamPush);
        BML_REGISTER_API_GUARDED(
            bmlImcStreamComplete, "imc", BML_API_ImcStreamComplete);
        BML_REGISTER_API_GUARDED(bmlImcStreamError, "imc", BML_API_ImcStreamError);
        BML_REGISTER_API_GUARDED(
            bmlImcCallStreamingRpc, "imc", BML_API_ImcCallStreamingRpc);
    }
} // namespace BML::Core
