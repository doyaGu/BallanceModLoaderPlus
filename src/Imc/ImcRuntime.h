#ifndef BML_IMCRUNTIME_H
#define BML_IMCRUNTIME_H

#include "BML/Imc.h"

#include <cstddef>
#include <memory>
#include <string>
#include <thread>

namespace BML {

class ModInvocationGate;

class ImcRuntime {
public:
    explicit ImcRuntime(ModInvocationGate *invocationGate = nullptr);
    ~ImcRuntime();

    ImcRuntime(const ImcRuntime &) = delete;
    ImcRuntime &operator=(const ImcRuntime &) = delete;

    int OpenClient(const std::string &ownerId, BML_ImcClient *outClient);
    int CloseClient(BML_ImcClient client);

    int GetRpcId(BML_ImcClient client, const char *name, BML_ImcRpcId *outId);
    int GetTopicId(BML_ImcClient client, const char *name, BML_ImcTopicId *outId);
    int GetPayloadTypeId(BML_ImcClient client, const char *name,
                         BML_ImcPayloadTypeId *outId);
    int IsRpcAvailable(BML_ImcClient client, BML_ImcRpcId rpcId,
                       int *outAvailable);

    int RegisterRpc(BML_ImcClient client, BML_ImcRpcId rpcId,
                    const BML_ImcRpcRegistrationOptions *options,
                    BML_ImcRpcHandler handler, void *userdata);
    int UnregisterRpc(BML_ImcClient client, BML_ImcRpcId rpcId);
    int CallRpc(BML_ImcClient client, BML_ImcRpcId rpcId,
                const BML_ImcMessage *request, const BML_ImcCallOptions *options,
                BML_ImcFuture *outFuture);

    int FutureGetState(BML_ImcFuture future, BML_ImcFutureState *outState);
    int FutureAwait(BML_ImcFuture future, uint32_t timeoutMs);
    int FutureCancel(BML_ImcFuture future);
    int FutureGetResult(BML_ImcFuture future, BML_ImcMessage *outMessage);
    int FutureGetError(BML_ImcFuture future, int *outError);
    int FutureOnComplete(BML_ImcClient client, BML_ImcFuture future,
                         BML_ImcFutureCallback callback, void *userdata);
    int FutureRelease(BML_ImcFuture future);

    int Subscribe(BML_ImcClient client, BML_ImcTopicId topicId,
                  const BML_ImcSubscribeOptions *options,
                  BML_ImcTopicHandler handler, void *userdata,
                  BML_ImcSubscription *outSubscription);
    int Unsubscribe(BML_ImcClient client, BML_ImcSubscription subscription);
    int GetSubscriptionDroppedCount(BML_ImcClient client,
                                    BML_ImcSubscription subscription,
                                    uint64_t *outCount);
    int Publish(BML_ImcClient client, BML_ImcTopicId topicId,
                const BML_ImcMessage *message, size_t *outDelivered);
    int GetTopicSubscriberCount(BML_ImcClient client, BML_ImcTopicId topicId,
                                size_t *outCount);

    int GetStats(BML_ImcClient client, BML_ImcStats *outStats);

    void Pump(size_t rpcBudget = 256,
              size_t messageBudgetPerSubscription = 256,
              size_t completionBudget = 256);
    void CleanupOwner(const std::string &ownerId);
    void Shutdown();

    bool IsMainThread() const noexcept;
    void SetInvocationGate(ModInvocationGate *invocationGate) noexcept;

    static int ResponseReserve(BML_ImcResponse *response, size_t size, void **outData);
    static int ResponseCommit(BML_ImcResponse *response, size_t size,
                              BML_ImcPayloadTypeId payloadType);
    static int ResponseWrite(BML_ImcResponse *response, const void *data, size_t size,
                             BML_ImcPayloadTypeId payloadType);

private:
    struct State;
    std::unique_ptr<State> m_State;
};

} // namespace BML

#endif // BML_IMCRUNTIME_H
