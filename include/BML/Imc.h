// The C ABI two Mods talk to each other over, and the layer everything else about IMC is
// built on. Nothing but C scalars, fixed-layout structs, byte spans, function pointers,
// and opaque handles crosses this boundary, which is what lets a Mod built with another
// compiler, another standard library, or another language use an API a Mod publishes,
// where handing over a C++ object or an STL container could not.
//
// There are two operations and no more: an RPC, one caller to one handler with an answer
// coming back, and a Topic, one publisher to any number of subscribers with nothing
// coming back. Everything else, a query, a command, a collection, a notification, is one
// of those two carrying a payload the two sides agreed on.
//
// This is the bottom layer, and most Mods should be a layer or two above it. A generated
// *_imc.hpp from a .imc interface gives typed calls and does the encoding, the id
// caching, and the handle lifetime; ImcCpp.hpp gives C++ wrappers for a Mod integrating
// something of its own; Scene.h, Gameplay.h, UI.h, and Events.h are already written on
// top of it for what the loader itself offers. Runtime.h and Speedrun.h answer out of a
// versioned interface struct instead of over IMC, which is what Interface.h is about.
// Reach for the functions here when writing that kind of layer, not when using one.
// docs/en/imc.md is the whole picture and imc-author-guide.md is how an interface is
// written.
//
// What this header cannot show:
//
// Every function answers with an int from Defines.h, BML_OK for success and a negative
// code otherwise, and never throws: an exception inside the loader becomes
// BML_ERROR_FAIL or BML_ERROR_OUT_OF_MEMORY. Output is written through the out
// parameters and only on success.
//
// Every struct here starts with Size and the loader reads it, both to tell which version
// of the struct it was handed and to refuse one that is too small. Fill a struct from its
// _INIT macro and change what is needed, rather than zeroing it, and set Size on an
// output struct too: BML_Imc_FutureGetResult and BML_Imc_GetStats read it before writing
// anything.
//
// A client is opened once per Mod and stands for that Mod's IMC state. Pass null for
// ownerId and the loader works out which Mod's DLL called; pass a name and it has to be
// that Mod's own, or the call is refused. Everything else takes the client, which is how
// the loader knows whose registrations to revoke when the Mod unloads.
//
// Names become ids through the three Get*Id functions, and an id is a cache key inside
// this process, not something to store or send anywhere. Those functions make the id up
// on the spot for a name nobody has used, so an id in hand says nothing about anyone
// being there to answer: BML_Imc_IsRpcAvailable is a snapshot, and a call still has to
// handle BML_ERROR_IMC_ENDPOINT_NOT_FOUND.
//
// Which thread a handler runs on is chosen when it is registered, not when it is called.
// GAME_THREAD queues the work for the loader's pump, which is the only way to touch
// Virtools objects, the loader's UI, or anything else of the game's. CALLER_THREAD runs
// it inline on whichever thread called, so it has to be short and safe to run beside
// itself, since several callers can be inside it at once. A GAME_THREAD handler called
// from the game thread is the one case that is neither: it runs inline as well, since the
// caller is already where the work belongs, and the future comes back ready.
//
// That is also what makes waiting safe or not. Waiting for the pump from the pump would
// deadlock, so a nonzero-timeout wait on a future that is still pending answers
// BML_ERROR_WRONG_THREAD on the game thread; a wait on one that is already ready, which
// is what a game-thread call to a game-thread handler leaves behind, returns the result.
// A zero timeout is a poll and is allowed anywhere, answering BML_ERROR_BUSY while the
// call is still pending.
//
// Bytes are borrowed everywhere. The Data of a BML_ImcMessage handed to a handler is good
// only until that handler returns, and the one from BML_Imc_FutureGetResult only until
// the future is released, so copy out anything to be kept. A response is written from
// inside the handler, either by reserving space and committing what was written or by
// handing over a buffer to copy, and not after the handler has returned.
//
// Handles are dead the moment they are released. Closing a client, unsubscribing, or
// releasing a future invalidates that token at once, even where the loader still has
// queued work of its own to finish, and using it afterwards is BML_ERROR_INVALID_HANDLE,
// which is a fault in the Mod rather than something to retry. Closing a client or a
// subscription from inside one of its own callbacks is allowed and stops further dispatch
// at once, with the removal itself left until the outermost callback returns.
//
// Shutting down is the Mod's job and the order matters: stop calling and publishing,
// cancel or release the futures still out, close the subscriptions before freeing what
// their callbacks read, unregister the handlers, then close the client. The loader
// revokes what is left when the Mod unloads, but a callback still running while its data
// goes away is not something it can save.
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
