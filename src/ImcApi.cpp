#include "BML/Imc.h"

#include "ImcRuntime.h"
#include "ModContext.h"

#include <intrin.h>
#include <new>
#include <string>

namespace {

BML::ImcRuntime *CurrentRuntime() noexcept {
    auto *context = BML_GetModContext();
    return context ? &context->GetImcRuntime() : nullptr;
}

template <typename Function>
int GuardImc(Function &&function) noexcept {
    try {
        return function();
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

} // namespace

BML_EXPORT int BML_Imc_OpenClient(const char *ownerId, BML_ImcClient *outClient) {
    const void *callerReturnAddress = _ReturnAddress();
    return GuardImc([&, callerReturnAddress] {
        if (!outClient)
            return BML_ERROR_INVALID_PARAMETER;
        *outClient = nullptr;
        auto *context = BML_GetModContext();
        if (!context)
            return BML_ERROR_FROZEN;
        const std::string resolved = context->GetNativeInteropOwnerId(
            callerReturnAddress, ownerId);
        if (resolved.empty() || (ownerId && resolved != ownerId))
            return BML_ERROR_ACCESS_DENIED;
        return context->GetImcRuntime().OpenClient(resolved, outClient);
    });
}

BML_EXPORT int BML_Imc_CloseClient(BML_ImcClient client) {
    return GuardImc([&] {
        auto *runtime = CurrentRuntime();
        return runtime ? runtime->CloseClient(client) : BML_ERROR_FROZEN;
    });
}

BML_EXPORT int BML_Imc_GetRpcId(BML_ImcClient client, const char *name,
                                BML_ImcRpcId *outId) {
    return GuardImc([&] {
        auto *runtime = CurrentRuntime();
        return runtime ? runtime->GetRpcId(client, name, outId) : BML_ERROR_FROZEN;
    });
}

BML_EXPORT int BML_Imc_GetTopicId(BML_ImcClient client, const char *name,
                                  BML_ImcTopicId *outId) {
    return GuardImc([&] {
        auto *runtime = CurrentRuntime();
        return runtime ? runtime->GetTopicId(client, name, outId) : BML_ERROR_FROZEN;
    });
}

BML_EXPORT int BML_Imc_GetPayloadTypeId(BML_ImcClient client, const char *name,
                                        BML_ImcPayloadTypeId *outId) {
    return GuardImc([&] {
        auto *runtime = CurrentRuntime();
        return runtime ? runtime->GetPayloadTypeId(client, name, outId) : BML_ERROR_FROZEN;
    });
}

BML_EXPORT int BML_Imc_IsRpcAvailable(BML_ImcClient client,
                                      BML_ImcRpcId rpcId,
                                      int *outAvailable) {
    return GuardImc([&] {
        auto *runtime = CurrentRuntime();
        return runtime ? runtime->IsRpcAvailable(client, rpcId, outAvailable)
                       : BML_ERROR_FROZEN;
    });
}

BML_EXPORT int BML_Imc_RegisterRpc(BML_ImcClient client, BML_ImcRpcId rpcId,
                                   const BML_ImcRpcRegistrationOptions *options,
                                   BML_ImcRpcHandler handler, void *userdata) {
    return GuardImc([&] {
        auto *runtime = CurrentRuntime();
        return runtime ? runtime->RegisterRpc(client, rpcId, options, handler, userdata)
                       : BML_ERROR_FROZEN;
    });
}

BML_EXPORT int BML_Imc_UnregisterRpc(BML_ImcClient client, BML_ImcRpcId rpcId) {
    return GuardImc([&] {
        auto *runtime = CurrentRuntime();
        return runtime ? runtime->UnregisterRpc(client, rpcId) : BML_ERROR_FROZEN;
    });
}

BML_EXPORT int BML_Imc_CallRpc(BML_ImcClient client, BML_ImcRpcId rpcId,
                               const BML_ImcMessage *request,
                               const BML_ImcCallOptions *options,
                               BML_ImcFuture *outFuture) {
    return GuardImc([&] {
        auto *runtime = CurrentRuntime();
        return runtime ? runtime->CallRpc(client, rpcId, request, options, outFuture)
                       : BML_ERROR_FROZEN;
    });
}

BML_EXPORT int BML_Imc_ResponseReserve(BML_ImcResponse *response, size_t size,
                                       void **outData) {
    return GuardImc([&] { return BML::ImcRuntime::ResponseReserve(response, size, outData); });
}

BML_EXPORT int BML_Imc_ResponseCommit(BML_ImcResponse *response, size_t size,
                                      BML_ImcPayloadTypeId payloadType) {
    return GuardImc([&] { return BML::ImcRuntime::ResponseCommit(response, size, payloadType); });
}

BML_EXPORT int BML_Imc_ResponseWrite(BML_ImcResponse *response, const void *data,
                                     size_t size, BML_ImcPayloadTypeId payloadType) {
    return GuardImc([&] { return BML::ImcRuntime::ResponseWrite(response, data, size, payloadType); });
}

BML_EXPORT int BML_Imc_FutureGetState(BML_ImcFuture future,
                                      BML_ImcFutureState *outState) {
    return GuardImc([&] {
        auto *runtime = CurrentRuntime();
        return runtime ? runtime->FutureGetState(future, outState) : BML_ERROR_FROZEN;
    });
}

BML_EXPORT int BML_Imc_FutureAwait(BML_ImcFuture future, uint32_t timeoutMs) {
    return GuardImc([&] {
        auto *runtime = CurrentRuntime();
        return runtime ? runtime->FutureAwait(future, timeoutMs) : BML_ERROR_FROZEN;
    });
}

BML_EXPORT int BML_Imc_FutureCancel(BML_ImcFuture future) {
    return GuardImc([&] {
        auto *runtime = CurrentRuntime();
        return runtime ? runtime->FutureCancel(future) : BML_ERROR_FROZEN;
    });
}

BML_EXPORT int BML_Imc_FutureGetResult(BML_ImcFuture future,
                                       BML_ImcMessage *outMessage) {
    return GuardImc([&] {
        auto *runtime = CurrentRuntime();
        return runtime ? runtime->FutureGetResult(future, outMessage) : BML_ERROR_FROZEN;
    });
}

BML_EXPORT int BML_Imc_FutureGetError(BML_ImcFuture future, int *outError) {
    return GuardImc([&] {
        auto *runtime = CurrentRuntime();
        return runtime ? runtime->FutureGetError(future, outError) : BML_ERROR_FROZEN;
    });
}

BML_EXPORT int BML_Imc_FutureOnComplete(BML_ImcClient client,
                                        BML_ImcFuture future,
                                        BML_ImcFutureCallback callback,
                                        void *userdata) {
    return GuardImc([&] {
        auto *runtime = CurrentRuntime();
        return runtime ? runtime->FutureOnComplete(client, future, callback, userdata)
                       : BML_ERROR_FROZEN;
    });
}

BML_EXPORT int BML_Imc_FutureRelease(BML_ImcFuture future) {
    return GuardImc([&] {
        auto *runtime = CurrentRuntime();
        return runtime ? runtime->FutureRelease(future) : BML_ERROR_FROZEN;
    });
}

BML_EXPORT int BML_Imc_Subscribe(BML_ImcClient client, BML_ImcTopicId topicId,
                                 const BML_ImcSubscribeOptions *options,
                                 BML_ImcTopicHandler handler, void *userdata,
                                 BML_ImcSubscription *outSubscription) {
    return GuardImc([&] {
        auto *runtime = CurrentRuntime();
        return runtime ? runtime->Subscribe(client, topicId, options, handler, userdata,
                                            outSubscription)
                       : BML_ERROR_FROZEN;
    });
}

BML_EXPORT int BML_Imc_Unsubscribe(BML_ImcClient client,
                                   BML_ImcSubscription subscription) {
    return GuardImc([&] {
        auto *runtime = CurrentRuntime();
        return runtime ? runtime->Unsubscribe(client, subscription) : BML_ERROR_FROZEN;
    });
}

BML_EXPORT int BML_Imc_GetSubscriptionDroppedCount(BML_ImcClient client,
                                                    BML_ImcSubscription subscription,
                                                    uint64_t *outCount) {
    return GuardImc([&] {
        auto *runtime = CurrentRuntime();
        return runtime ? runtime->GetSubscriptionDroppedCount(client, subscription, outCount)
                       : BML_ERROR_FROZEN;
    });
}
BML_EXPORT int BML_Imc_Publish(BML_ImcClient client, BML_ImcTopicId topicId,
                               const BML_ImcMessage *message,
                               size_t *outDelivered) {
    return GuardImc([&] {
        auto *runtime = CurrentRuntime();
        return runtime ? runtime->Publish(client, topicId, message, outDelivered)
                       : BML_ERROR_FROZEN;
    });
}

BML_EXPORT int BML_Imc_GetTopicSubscriberCount(BML_ImcClient client,
                                                BML_ImcTopicId topicId,
                                                size_t *outCount) {
    return GuardImc([&] {
        auto *runtime = CurrentRuntime();
        return runtime ? runtime->GetTopicSubscriberCount(client, topicId, outCount)
                       : BML_ERROR_FROZEN;
    });
}
BML_EXPORT int BML_Imc_GetStats(BML_ImcClient client, BML_ImcStats *outStats) {
    return GuardImc([&] {
        auto *runtime = CurrentRuntime();
        return runtime ? runtime->GetStats(client, outStats) : BML_ERROR_FROZEN;
    });
}
