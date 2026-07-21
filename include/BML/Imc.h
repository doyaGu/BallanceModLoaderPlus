#ifndef BML_IMC_H
#define BML_IMC_H

#include "BML/Defines.h"

BML_BEGIN_CDECLS

#define BML_IMC_ABI_VERSION 1u
#define BML_IMC_INLINE_PAYLOAD_SIZE 256u

typedef uint32_t BML_ImcRpcId;
typedef uint32_t BML_ImcTopicId;
typedef uint32_t BML_ImcPayloadTypeId;

typedef struct BML_ImcClient_T *BML_ImcClient;
typedef struct BML_ImcFuture_T *BML_ImcFuture;
typedef struct BML_ImcSubscription_T *BML_ImcSubscription;
typedef struct BML_ImcResponse_T BML_ImcResponse;

#define BML_IMC_INVALID_ID 0u

typedef enum BML_ImcExecution {
    BML_IMC_EXECUTION_GAME_THREAD = 0,
    BML_IMC_EXECUTION_CALLER_THREAD = 1,
    _BML_IMC_EXECUTION_FORCE_32BIT = 0x7fffffff
} BML_ImcExecution;

typedef enum BML_ImcBackpressure {
    BML_IMC_BACKPRESSURE_DROP_OLDEST = 0,
    BML_IMC_BACKPRESSURE_DROP_NEWEST = 1,
    BML_IMC_BACKPRESSURE_FAIL = 2,
    _BML_IMC_BACKPRESSURE_FORCE_32BIT = 0x7fffffff
} BML_ImcBackpressure;

typedef enum BML_ImcFutureState {
    BML_IMC_FUTURE_PENDING = 0,
    BML_IMC_FUTURE_READY = 1,
    BML_IMC_FUTURE_FAILED = 2,
    BML_IMC_FUTURE_CANCELLED = 3,
    BML_IMC_FUTURE_TIMED_OUT = 4,
    _BML_IMC_FUTURE_STATE_FORCE_32BIT = 0x7fffffff
} BML_ImcFutureState;

typedef struct BML_ImcMessage {
    size_t Size;
    const void *Data;
    size_t DataSize;
    BML_ImcPayloadTypeId PayloadType;
    uint32_t Flags;
    uint64_t MessageId;
    uint64_t TimestampNs;
} BML_ImcMessage;

#define BML_IMC_MESSAGE_INIT \
    { sizeof(BML_ImcMessage), NULL, 0u, BML_IMC_INVALID_ID, 0u, 0u, 0u }

typedef struct BML_ImcRpcRegistrationOptions {
    size_t Size;
    BML_ImcExecution Execution;
    uint32_t Flags;
} BML_ImcRpcRegistrationOptions;

#define BML_IMC_RPC_REGISTRATION_OPTIONS_INIT \
    { sizeof(BML_ImcRpcRegistrationOptions), BML_IMC_EXECUTION_GAME_THREAD, 0u }

typedef struct BML_ImcCallOptions {
    size_t Size;
    uint32_t TimeoutMs;
    uint32_t Flags;
} BML_ImcCallOptions;

#define BML_IMC_CALL_OPTIONS_INIT \
    { sizeof(BML_ImcCallOptions), 5000u, 0u }

typedef struct BML_ImcSubscribeOptions {
    size_t Size;
    BML_ImcExecution Execution;
    BML_ImcBackpressure Backpressure;
    uint32_t Capacity;
    BML_ImcPayloadTypeId ExpectedPayloadType;
    uint32_t Flags;
} BML_ImcSubscribeOptions;

#define BML_IMC_SUBSCRIBE_OPTIONS_INIT \
    { sizeof(BML_ImcSubscribeOptions), BML_IMC_EXECUTION_GAME_THREAD, \
      BML_IMC_BACKPRESSURE_DROP_OLDEST, 256u, BML_IMC_INVALID_ID, 0u }

typedef struct BML_ImcStats {
    size_t Size;
    uint64_t RpcCalls;
    uint64_t RpcCompleted;
    uint64_t RpcFailed;
    uint64_t RpcQueueFull;
    uint64_t MessagesPublished;
    uint64_t MessagesDelivered;
    uint64_t MessagesDropped;
    uint32_t ActiveRpcHandlers;
    uint32_t ActiveSubscriptions;
    uint32_t PendingRpcCalls;
} BML_ImcStats;

typedef int (*BML_ImcRpcHandler)(BML_ImcRpcId rpcId,
                                 const BML_ImcMessage *request,
                                 BML_ImcResponse *response,
                                 void *userdata);

typedef void (*BML_ImcTopicHandler)(BML_ImcTopicId topicId,
                                    const BML_ImcMessage *message,
                                    void *userdata);

typedef void (*BML_ImcFutureCallback)(BML_ImcFuture future, void *userdata);

/* ownerId may be NULL for automatic caller-DLL ownership resolution.  When it
 * is non-NULL, BML verifies that the caller DLL owns that mod ID. */
BML_EXPORT int BML_Imc_OpenClient(const char *ownerId, BML_ImcClient *outClient);
/* A successful close invalidates the public token immediately. */
BML_EXPORT int BML_Imc_CloseClient(BML_ImcClient client);

BML_EXPORT int BML_Imc_GetRpcId(BML_ImcClient client, const char *name,
                                BML_ImcRpcId *outId);
BML_EXPORT int BML_Imc_GetTopicId(BML_ImcClient client, const char *name,
                                  BML_ImcTopicId *outId);
BML_EXPORT int BML_Imc_GetPayloadTypeId(BML_ImcClient client, const char *name,
                                        BML_ImcPayloadTypeId *outId);
/* Returns a point-in-time handler snapshot for a known RPC ID. Registration
 * may change immediately after this call; callers must still handle an
 * endpoint-not-found result from BML_Imc_CallRpc. */
BML_EXPORT int BML_Imc_IsRpcAvailable(BML_ImcClient client,
                                      BML_ImcRpcId rpcId,
                                      int *outAvailable);

BML_EXPORT int BML_Imc_RegisterRpc(BML_ImcClient client, BML_ImcRpcId rpcId,
                                   const BML_ImcRpcRegistrationOptions *options,
                                   BML_ImcRpcHandler handler, void *userdata);
BML_EXPORT int BML_Imc_UnregisterRpc(BML_ImcClient client, BML_ImcRpcId rpcId);
BML_EXPORT int BML_Imc_CallRpc(BML_ImcClient client, BML_ImcRpcId rpcId,
                               const BML_ImcMessage *request,
                               const BML_ImcCallOptions *options,
                               BML_ImcFuture *outFuture);

BML_EXPORT int BML_Imc_ResponseReserve(BML_ImcResponse *response, size_t size,
                                       void **outData);
BML_EXPORT int BML_Imc_ResponseCommit(BML_ImcResponse *response, size_t size,
                                      BML_ImcPayloadTypeId payloadType);
BML_EXPORT int BML_Imc_ResponseWrite(BML_ImcResponse *response, const void *data,
                                     size_t size, BML_ImcPayloadTypeId payloadType);

BML_EXPORT int BML_Imc_FutureGetState(BML_ImcFuture future,
                                      BML_ImcFutureState *outState);
/* timeoutMs == 0 is a non-blocking poll and is safe on the game thread.
 * A pending wait with a nonzero timeout is rejected on the game thread. */
BML_EXPORT int BML_Imc_FutureAwait(BML_ImcFuture future, uint32_t timeoutMs);
BML_EXPORT int BML_Imc_FutureCancel(BML_ImcFuture future);
BML_EXPORT int BML_Imc_FutureGetResult(BML_ImcFuture future,
                                       BML_ImcMessage *outMessage);
BML_EXPORT int BML_Imc_FutureGetError(BML_ImcFuture future, int *outError);
BML_EXPORT int BML_Imc_FutureOnComplete(BML_ImcClient client,
                                        BML_ImcFuture future,
                                        BML_ImcFutureCallback callback,
                                        void *userdata);
/* Invalidates the public token immediately. Queued runtime work may retain the
 * private state until completion, but this token cannot be used or revived. */
BML_EXPORT int BML_Imc_FutureRelease(BML_ImcFuture future);

BML_EXPORT int BML_Imc_Subscribe(BML_ImcClient client, BML_ImcTopicId topicId,
                                 const BML_ImcSubscribeOptions *options,
                                 BML_ImcTopicHandler handler, void *userdata,
                                 BML_ImcSubscription *outSubscription);
/* A successful unsubscribe invalidates the public token immediately. */
BML_EXPORT int BML_Imc_Unsubscribe(BML_ImcClient client,
                                   BML_ImcSubscription subscription);
BML_EXPORT int BML_Imc_GetSubscriptionDroppedCount(BML_ImcClient client,
                                                    BML_ImcSubscription subscription,
                                                    uint64_t *outCount);
BML_EXPORT int BML_Imc_Publish(BML_ImcClient client, BML_ImcTopicId topicId,
                               const BML_ImcMessage *message,
                               size_t *outDelivered);
BML_EXPORT int BML_Imc_GetTopicSubscriberCount(BML_ImcClient client,
                                                BML_ImcTopicId topicId,
                                                size_t *outCount);

BML_EXPORT int BML_Imc_GetStats(BML_ImcClient client, BML_ImcStats *outStats);

BML_END_CDECLS

#endif // BML_IMC_H
