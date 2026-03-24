#include "ImcBusInternal.h"

namespace BML::Core {
    namespace {
        ImcBusImpl *TryGetBusFromKernel(KernelServices *kernel) {
            if (!kernel) {
                return nullptr;
            }
            return &GetBus(*kernel);
        }

        ImcBusImpl *TryGetBusFromOwner(BML_Mod owner) {
            return TryGetBusFromKernel(Context::KernelFromMod(owner));
        }

        ImcBusImpl *TryGetBusFromSubscription(BML_Subscription sub) {
            auto *subscription = static_cast<BML_Subscription_T *>(sub);
            if (!subscription) {
                return nullptr;
            }
            if (subscription->kernel) {
                return TryGetBusFromKernel(subscription->kernel);
            }
            return TryGetBusFromOwner(subscription->owner);
        }

        ImcBusImpl *TryGetBusFromFuture(BML_Future future) {
            if (!future) {
                return nullptr;
            }
            return TryGetBusFromKernel(future->kernel);
        }

        ImcBusImpl *TryGetBusFromStream(BML_RpcStream stream) {
            auto *rpcStream = static_cast<BML_RpcStream_T *>(stream);
            if (!rpcStream) {
                return nullptr;
            }
            if (rpcStream->kernel) {
                return TryGetBusFromKernel(rpcStream->kernel);
            }
            return rpcStream->future ? TryGetBusFromFuture(rpcStream->future) : nullptr;
        }

        ImcBusImpl *TryGetAmbientBus() {
            return nullptr;
        }
    } // namespace

    void ImcPump(size_t max_per_sub) {
        if (auto *bus = TryGetAmbientBus()) {
            bus->Pump(max_per_sub);
        }
    }
    void ImcPump(KernelServices &kernel, size_t max_per_sub) { GetBus(kernel).Pump(max_per_sub); }
    void ImcShutdown() {
        if (auto *bus = TryGetAmbientBus()) {
            bus->Shutdown();
        }
    }
    void ImcCleanupOwner(BML_Mod owner) {
        auto *bus = TryGetBusFromOwner(owner);
        if (bus) {
            bus->CleanupOwner(owner);
        }
    }
    void ImcBindDeps(Context &ctx) {
        auto *kernel = Context::KernelFromHandle(ctx.GetHandle());
        if (kernel) {
            GetBus(*kernel).BindDeps(ctx);
        }
    }

    BML_Result ImcGetTopicId(const char *n, BML_TopicId *o) {
        auto *bus = TryGetAmbientBus();
        return bus ? bus->GetTopicId(n, o) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcGetTopicId(KernelServices &kernel, const char *n, BML_TopicId *o) {
        return GetBus(kernel).GetTopicId(n, o);
    }
    BML_Result ImcGetRpcId(const char *n, BML_RpcId *o) {
        auto *bus = TryGetAmbientBus();
        return bus ? bus->GetRpcId(n, o) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcGetRpcId(KernelServices &kernel, const char *n, BML_RpcId *o) {
        return GetBus(kernel).GetRpcId(n, o);
    }
    BML_Result ImcPublish(BML_TopicId t, const void *d, size_t s) {
        auto *bus = TryGetAmbientBus();
        return bus ? bus->Publish(t, d, s) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcPublish(KernelServices &kernel, BML_TopicId t, const void *d, size_t s) {
        return GetBus(kernel).Publish(t, d, s);
    }
    BML_Result ImcPublish(BML_Mod owner, BML_TopicId t, const void *d, size_t s) {
        auto *bus = TryGetBusFromOwner(owner);
        return bus ? bus->Publish(owner, t, d, s) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcPublishEx(BML_TopicId t, const BML_ImcMessage *m) {
        auto *bus = TryGetAmbientBus();
        return bus ? bus->PublishEx(t, m) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcPublishEx(BML_Mod owner, BML_TopicId t, const BML_ImcMessage *m) {
        auto *bus = TryGetBusFromOwner(owner);
        return bus ? bus->PublishEx(owner, t, m) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcPublishBuffer(BML_TopicId t, const BML_ImcBuffer *b) {
        auto *bus = TryGetAmbientBus();
        return bus ? bus->PublishBuffer(t, b) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcPublishBuffer(BML_Mod owner, BML_TopicId t, const BML_ImcBuffer *b) {
        auto *bus = TryGetBusFromOwner(owner);
        return bus ? bus->PublishBuffer(owner, t, b) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcSubscribe(BML_TopicId t, BML_ImcHandler h, void *u,
                            BML_Subscription *o) {
        auto *bus = TryGetAmbientBus();
        return bus ? bus->Subscribe(t, h, u, o) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcSubscribe(BML_Mod owner, BML_TopicId t, BML_ImcHandler h, void *u, BML_Subscription *o) {
        auto *bus = TryGetBusFromOwner(owner);
        return bus ? bus->Subscribe(owner, t, h, u, o) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcSubscribeEx(BML_TopicId t, BML_ImcHandler h, void *u,
                              const BML_SubscribeOptions *opts,
                              BML_Subscription *o) {
        auto *bus = TryGetAmbientBus();
        return bus ? bus->SubscribeEx(t, h, u, opts, o) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcSubscribeEx(BML_Mod owner, BML_TopicId t, BML_ImcHandler h,
                              void *u, const BML_SubscribeOptions *opts,
                              BML_Subscription *o) {
        auto *bus = TryGetBusFromOwner(owner);
        return bus ? bus->SubscribeEx(owner, t, h, u, opts, o) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcUnsubscribe(BML_Subscription s) {
        auto *bus = TryGetBusFromSubscription(s);
        return bus ? bus->Unsubscribe(s) : BML_RESULT_INVALID_HANDLE;
    }
    BML_Result ImcSubscriptionIsActive(BML_Subscription s, BML_Bool *o) {
        auto *bus = TryGetBusFromSubscription(s);
        return bus ? bus->SubscriptionIsActive(s, o) : BML_RESULT_INVALID_HANDLE;
    }
    BML_Result ImcGetSubscriptionStats(BML_Subscription s, BML_SubscriptionStats *o) {
        auto *bus = TryGetBusFromSubscription(s);
        return bus ? bus->GetSubscriptionStats(s, o) : BML_RESULT_INVALID_HANDLE;
    }
    BML_Result ImcSubscribeIntercept(BML_TopicId t, BML_ImcInterceptHandler h,
                                     void *u, BML_Subscription *o) {
        auto *bus = TryGetAmbientBus();
        return bus ? bus->SubscribeIntercept(t, h, u, o) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcSubscribeIntercept(BML_Mod owner, BML_TopicId t,
                                     BML_ImcInterceptHandler h, void *u,
                                     BML_Subscription *o) {
        auto *bus = TryGetBusFromOwner(owner);
        return bus ? bus->SubscribeIntercept(owner, t, h, u, o) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcSubscribeInterceptEx(BML_TopicId t, BML_ImcInterceptHandler h,
                                       void *u, const BML_SubscribeOptions *opts,
                                       BML_Subscription *o) {
        auto *bus = TryGetAmbientBus();
        return bus ? bus->SubscribeInterceptEx(t, h, u, opts, o)
                   : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcSubscribeInterceptEx(BML_Mod owner, BML_TopicId t,
                                       BML_ImcInterceptHandler h, void *u,
                                       const BML_SubscribeOptions *opts,
                                       BML_Subscription *o) {
        auto *bus = TryGetBusFromOwner(owner);
        return bus ? bus->SubscribeInterceptEx(owner, t, h, u, opts, o)
                   : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcPublishInterceptable(BML_TopicId t, BML_ImcMessage *m, BML_EventResult *o) {
        auto *bus = TryGetAmbientBus();
        return bus ? bus->PublishInterceptable(t, m, o) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcPublishInterceptable(BML_Mod owner, BML_TopicId t, BML_ImcMessage *m,
                                       BML_EventResult *o) {
        auto *bus = TryGetBusFromOwner(owner);
        return bus ? bus->PublishInterceptable(owner, t, m, o) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcPublishMulti(const BML_TopicId *ts, size_t n, const void *d, size_t s,
                               const BML_ImcMessage *m, size_t *o) {
        auto *bus = TryGetAmbientBus();
        return bus ? bus->PublishMulti(ts, n, d, s, m, o) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcPublishMulti(BML_Mod owner, const BML_TopicId *ts, size_t n,
                               const void *d, size_t s, const BML_ImcMessage *m,
                               size_t *o) {
        auto *bus = TryGetBusFromOwner(owner);
        return bus ? bus->PublishMulti(owner, ts, n, d, s, m, o) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcRegisterRpc(BML_RpcId r, BML_RpcHandler h, void *u) {
        auto *bus = TryGetAmbientBus();
        return bus ? bus->RegisterRpc(r, h, u) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcRegisterRpc(BML_Mod owner, BML_RpcId r, BML_RpcHandler h, void *u) {
        auto *bus = TryGetBusFromOwner(owner);
        return bus ? bus->RegisterRpc(owner, r, h, u) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcUnregisterRpc(BML_RpcId r) {
        auto *bus = TryGetAmbientBus();
        return bus ? bus->UnregisterRpc(r) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcUnregisterRpc(BML_Mod owner, BML_RpcId r) {
        auto *bus = TryGetBusFromOwner(owner);
        return bus ? bus->UnregisterRpc(owner, r) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcCallRpc(BML_RpcId r, const BML_ImcMessage *req, BML_Future *o) {
        auto *bus = TryGetAmbientBus();
        return bus ? bus->CallRpc(r, req, o) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcCallRpc(BML_Mod owner, BML_RpcId r, const BML_ImcMessage *req, BML_Future *o) {
        auto *bus = TryGetBusFromOwner(owner);
        return bus ? bus->CallRpc(owner, r, req, o) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcFutureAwait(BML_Future f, uint32_t t) {
        auto *bus = TryGetBusFromFuture(f);
        return bus ? bus->FutureAwait(f, t) : BML_RESULT_INVALID_HANDLE;
    }
    BML_Result ImcFutureGetResult(BML_Future f, BML_ImcMessage *o) {
        auto *bus = TryGetBusFromFuture(f);
        return bus ? bus->FutureGetResult(f, o) : BML_RESULT_INVALID_HANDLE;
    }
    BML_Result ImcFutureGetState(BML_Future f, BML_FutureState *o) {
        auto *bus = TryGetBusFromFuture(f);
        return bus ? bus->FutureGetState(f, o) : BML_RESULT_INVALID_HANDLE;
    }
    BML_Result ImcFutureCancel(BML_Future f) {
        auto *bus = TryGetBusFromFuture(f);
        return bus ? bus->FutureCancel(f) : BML_RESULT_INVALID_HANDLE;
    }
    BML_Result ImcFutureOnComplete(BML_Future f, BML_FutureCallback cb, void *u) {
        auto *bus = TryGetBusFromFuture(f);
        return bus ? bus->FutureOnComplete(f, cb, u) : BML_RESULT_INVALID_HANDLE;
    }
    BML_Result ImcFutureOnComplete(BML_Mod owner, BML_Future f,
                                   BML_FutureCallback cb, void *u) {
        auto *bus = TryGetBusFromFuture(f);
        return bus ? bus->FutureOnComplete(owner, f, cb, u) : BML_RESULT_INVALID_HANDLE;
    }
    BML_Result ImcFutureRelease(BML_Future f) {
        auto *bus = TryGetBusFromFuture(f);
        return bus ? bus->FutureRelease(f) : BML_RESULT_INVALID_HANDLE;
    }
    BML_Result ImcGetStats(BML_ImcStats *o) {
        auto *bus = TryGetAmbientBus();
        return bus ? bus->GetStats(o) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcGetStats(KernelServices &kernel, BML_ImcStats *o) { return GetBus(kernel).GetStats(o); }
    BML_Result ImcResetStats() {
        auto *bus = TryGetAmbientBus();
        return bus ? bus->ResetStats() : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcResetStats(KernelServices &kernel) { return GetBus(kernel).ResetStats(); }
    BML_Result ImcGetTopicInfo(BML_TopicId t, BML_TopicInfo *o) {
        auto *bus = TryGetAmbientBus();
        return bus ? bus->GetTopicInfo(t, o) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcGetTopicInfo(KernelServices &kernel, BML_TopicId t, BML_TopicInfo *o) {
        return GetBus(kernel).GetTopicInfo(t, o);
    }
    BML_Result ImcGetTopicName(BML_TopicId t, char *b, size_t s, size_t *o) {
        auto *bus = TryGetAmbientBus();
        return bus ? bus->GetTopicName(t, b, s, o) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcGetTopicName(KernelServices &kernel, BML_TopicId t, char *b, size_t s, size_t *o) {
        return GetBus(kernel).GetTopicName(t, b, s, o);
    }
    BML_Result ImcPublishState(BML_TopicId t, const BML_ImcMessage *m) {
        auto *bus = TryGetAmbientBus();
        return bus ? bus->PublishState(t, m) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcPublishState(BML_Mod owner, BML_TopicId t, const BML_ImcMessage *m) {
        auto *bus = TryGetBusFromOwner(owner);
        return bus ? bus->PublishState(owner, t, m) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcCopyState(BML_TopicId t, void *d, size_t ds, size_t *os,
                            BML_ImcStateMeta *om) {
        auto *bus = TryGetAmbientBus();
        return bus ? bus->CopyState(t, d, ds, os, om) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcCopyState(KernelServices &kernel, BML_TopicId t, void *d, size_t ds,
                            size_t *os, BML_ImcStateMeta *om) {
        return GetBus(kernel).CopyState(t, d, ds, os, om);
    }
    BML_Result ImcClearState(BML_TopicId t) {
        auto *bus = TryGetAmbientBus();
        return bus ? bus->ClearState(t) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcClearState(KernelServices &kernel, BML_TopicId t) {
        return GetBus(kernel).ClearState(t);
    }

    BML_Result ImcRegisterRpcEx(BML_RpcId r, BML_RpcHandlerEx h, void *u) {
        auto *bus = TryGetAmbientBus();
        return bus ? bus->RegisterRpcEx(r, h, u) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcRegisterRpcEx(BML_Mod owner, BML_RpcId r, BML_RpcHandlerEx h, void *u) {
        auto *bus = TryGetBusFromOwner(owner);
        return bus ? bus->RegisterRpcEx(owner, r, h, u) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcCallRpcEx(BML_RpcId r, const BML_ImcMessage *req,
                            const BML_RpcCallOptions *opts, BML_Future *o) {
        auto *bus = TryGetAmbientBus();
        return bus ? bus->CallRpcEx(r, req, opts, o) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcCallRpcEx(BML_Mod owner, BML_RpcId r, const BML_ImcMessage *req,
                            const BML_RpcCallOptions *opts, BML_Future *o) {
        auto *bus = TryGetBusFromOwner(owner);
        return bus ? bus->CallRpcEx(owner, r, req, opts, o) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcFutureGetError(BML_Future f, BML_Result *c, char *m, size_t cap, size_t *ol) {
        auto *bus = TryGetBusFromFuture(f);
        return bus ? bus->FutureGetError(f, c, m, cap, ol) : BML_RESULT_INVALID_HANDLE;
    }
    BML_Result ImcGetRpcInfo(BML_RpcId r, BML_RpcInfo *o) {
        auto *bus = TryGetAmbientBus();
        return bus ? bus->GetRpcInfo(r, o) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcGetRpcInfo(KernelServices &kernel, BML_RpcId r, BML_RpcInfo *o) {
        return GetBus(kernel).GetRpcInfo(r, o);
    }
    BML_Result ImcGetRpcName(BML_RpcId r, char *b, size_t c, size_t *o) {
        auto *bus = TryGetAmbientBus();
        return bus ? bus->GetRpcName(r, b, c, o) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcGetRpcName(KernelServices &kernel, BML_RpcId r, char *b, size_t c, size_t *o) {
        return GetBus(kernel).GetRpcName(r, b, c, o);
    }
    void ImcEnumerateRpc(void(*cb)(BML_RpcId, const char *, BML_Bool, void *), void *u) {
        if (auto *bus = TryGetAmbientBus()) {
            bus->EnumerateRpc(cb, u);
        }
    }
    void ImcEnumerateRpc(KernelServices &kernel,
                         void(*cb)(BML_RpcId, const char *, BML_Bool, void *),
                         void *u) {
        GetBus(kernel).EnumerateRpc(cb, u);
    }
    BML_Result ImcAddRpcMiddleware(BML_RpcMiddleware mw, int32_t p, void *u) {
        auto *bus = TryGetAmbientBus();
        return bus ? bus->AddRpcMiddleware(mw, p, u) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcAddRpcMiddleware(BML_Mod owner, BML_RpcMiddleware mw,
                                   int32_t p, void *u) {
        auto *bus = TryGetBusFromOwner(owner);
        return bus ? bus->AddRpcMiddleware(owner, mw, p, u) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcRemoveRpcMiddleware(BML_RpcMiddleware mw) {
        auto *bus = TryGetAmbientBus();
        return bus ? bus->RemoveRpcMiddleware(mw) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcRemoveRpcMiddleware(BML_Mod owner, BML_RpcMiddleware mw) {
        auto *bus = TryGetBusFromOwner(owner);
        return bus ? bus->RemoveRpcMiddleware(owner, mw) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcRegisterStreamingRpc(BML_RpcId r, BML_StreamingRpcHandler h, void *u) {
        auto *bus = TryGetAmbientBus();
        return bus ? bus->RegisterStreamingRpc(r, h, u) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcRegisterStreamingRpc(BML_Mod owner, BML_RpcId r,
                                       BML_StreamingRpcHandler h, void *u) {
        auto *bus = TryGetBusFromOwner(owner);
        return bus ? bus->RegisterStreamingRpc(owner, r, h, u) : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcStreamPush(BML_RpcStream s, const void *d, size_t sz) {
        auto *bus = TryGetBusFromStream(s);
        return bus ? bus->StreamPush(s, d, sz) : BML_RESULT_INVALID_HANDLE;
    }
    BML_Result ImcStreamComplete(BML_RpcStream s) {
        auto *bus = TryGetBusFromStream(s);
        return bus ? bus->StreamComplete(s) : BML_RESULT_INVALID_HANDLE;
    }
    BML_Result ImcStreamError(BML_RpcStream s, BML_Result e, const char *m) {
        auto *bus = TryGetBusFromStream(s);
        return bus ? bus->StreamError(s, e, m) : BML_RESULT_INVALID_HANDLE;
    }
    BML_Result ImcCallStreamingRpc(BML_RpcId r, const BML_ImcMessage *req,
                                   BML_ImcHandler oc, BML_FutureCallback od,
                                   void *u, BML_Future *o) {
        auto *bus = TryGetAmbientBus();
        return bus ? bus->CallStreamingRpc(r, req, oc, od, u, o)
                   : BML_RESULT_INVALID_CONTEXT;
    }
    BML_Result ImcCallStreamingRpc(BML_Mod owner, BML_RpcId r,
                                   const BML_ImcMessage *req, BML_ImcHandler oc,
                                   BML_FutureCallback od, void *u,
                                   BML_Future *o) {
        auto *bus = TryGetBusFromOwner(owner);
        return bus ? bus->CallStreamingRpc(owner, r, req, oc, od, u, o)
                   : BML_RESULT_INVALID_CONTEXT;
    }
} // namespace BML::Core
