#ifndef BML_IMC_RPC_H
#define BML_IMC_RPC_H

#include "bml_imc_types.h"

/**
 * @file bml_imc_rpc.h
 * @brief IMC RPC API
 *
 * Provides the RPC (Remote Procedure Call) interface including handler
 * registration, async call/future management, extended RPC with error
 * details, middleware, streaming RPC, and introspection. All function
 * pointer types and the BML_ImcRpcInterface vtable are defined here.
 */

BML_BEGIN_CDECLS

/* ========================================================================
 * Core APIs - RPC ID Resolution
 * ======================================================================== */

/**
 * @brief Get or create an RPC ID from a string name.
 *
 * @param[in]  ctx    BML context
 * @param[in]  name   RPC name
 * @param[out] out_id Receives the RPC ID
 * @return BML_RESULT_OK on success
 *
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcGetRpcId)(BML_Context ctx,
                                          const char *name,
                                          BML_RpcId *out_id);

/* ========================================================================
 * Core APIs - RPC
 * ======================================================================== */

/**
 * @brief Register an RPC handler.
 *
 * @param[in] owner    Module handle (owner of registration)
 * @param[in] rpc_id   RPC ID resolved through the IMC RPC interface
 * @param[in] handler  Handler callback
 * @param[in] user_data Opaque pointer passed to handler
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_ALREADY_EXISTS if handler already registered
 *
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcRegisterRpc)(BML_Mod owner,
                                             BML_RpcId rpc_id,
                                             BML_RpcHandler handler,
                                             void *user_data);

/**
 * @brief Unregister an RPC handler.
 *
 * @param[in] owner  Module handle (owner of registration)
 * @param[in] rpc_id RPC ID
 * @return BML_RESULT_OK on success
 *
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcUnregisterRpc)(BML_Mod owner, BML_RpcId rpc_id);

/**
 * @brief Call an RPC asynchronously.
 *
 * @param[in]  owner      Module handle (owner of call)
 * @param[in]  rpc_id     RPC ID
 * @param[in]  request    Request message (data + size)
 * @param[out] out_future Receives future handle for result
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_NOT_FOUND if no handler registered
 *
 * @threadsafe Yes
 *
 * @code
 * BML_ImcMessage req = BML_IMC_MSG(&query, sizeof(query));
 * BML_Future future;
 * callRpc(get_health_rpc, &req, &future);
 *
 * futureAwait(future, 1000);
 *
 * BML_ImcMessage response;
 * futureGetResult(future, &response);
 * int health = *(int*)response.data;
 *
 * futureRelease(future);
 * @endcode
 */
typedef BML_Result (*PFN_BML_ImcCallRpc)(BML_Mod owner,
                                         BML_RpcId rpc_id,
                                         const BML_ImcMessage *request,
                                         BML_Future *out_future);

/* ========================================================================
 * Core APIs - Extended RPC
 * ======================================================================== */

/**
 * @brief Register an extended RPC handler with error detail support.
 */
typedef BML_Result (*PFN_BML_ImcRegisterRpcEx)(BML_Mod owner,
                                               BML_RpcId rpc_id,
                                               BML_RpcHandlerEx handler,
                                               void *user_data);

/**
 * @brief Call an RPC with extended options (timeout, flags).
 */
typedef BML_Result (*PFN_BML_ImcCallRpcEx)(BML_Mod owner,
                                           BML_RpcId rpc_id,
                                           const BML_ImcMessage *request,
                                           const BML_RpcCallOptions *options,
                                           BML_Future *out_future);

/* ========================================================================
 * Core APIs - Future Management
 * ======================================================================== */

/**
 * @brief Wait for a future to complete.
 *
 * @param[in] future     Future handle
 * @param[in] timeout_ms Timeout in milliseconds (0 = no timeout)
 * @return BML_RESULT_OK if future completed
 * @return BML_RESULT_TIMEOUT if timeout expired
 *
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcFutureAwait)(BML_Future future, uint32_t timeout_ms);

/**
 * @brief Get the result of a completed future.
 *
 * @param[in]  future      Future handle
 * @param[out] out_message Receives a borrowed view of the future-owned result.
 * The view remains valid until the future is released.
 * @return BML_RESULT_OK if result available
 * @return BML_RESULT_NOT_FOUND if future not yet complete
 *
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcFutureGetResult)(BML_Future future,
                                                  BML_ImcMessage *out_message);

/**
 * @brief Get the current state of a future.
 *
 * @param[in]  future    Future handle
 * @param[out] out_state Receives current state
 * @return BML_RESULT_OK on success
 *
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcFutureGetState)(BML_Future future,
                                                BML_FutureState *out_state);

/**
 * @brief Cancel a pending future.
 *
 * @param[in] future Future handle
 * @return BML_RESULT_OK on success
 *
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcFutureCancel)(BML_Future future);

/**
 * @brief Set a completion callback on a future.
 *
 * @param[in] owner    Module handle (owner of callback)
 * @param[in] future   Future handle
 * @param[in] callback Callback to invoke on completion
 * @param[in] user_data Opaque pointer passed to callback
 * @return BML_RESULT_OK on success
 *
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcFutureOnComplete)(BML_Mod owner,
                                                  BML_Future future,
                                                  BML_FutureCallback callback,
                                                  void *user_data);

/**
 * @brief Release a future handle.
 *
 * Must be called when done with the future to free resources.
 *
 * @param[in] future Future handle
 * @return BML_RESULT_OK on success
 *
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcFutureRelease)(BML_Future future);

/**
 * @brief Get error details from a completed future.
 *
 * @param[in]  future     Future handle
 * @param[out] out_code   Receives error code
 * @param[out] msg        Buffer for error message
 * @param[in]  cap        Size of message buffer
 * @param[out] out_len    Receives actual message length (may be NULL)
 * @return BML_RESULT_OK on success
 */
typedef BML_Result (*PFN_BML_ImcFutureGetError)(BML_Future future,
                                                 BML_Result *out_code,
                                                 char *msg, size_t cap,
                                                 size_t *out_len);

/* ========================================================================
 * Core APIs - RPC Introspection
 * ======================================================================== */

/**
 * @brief Get info about a registered RPC endpoint.
 */
typedef BML_Result (*PFN_BML_ImcGetRpcInfo)(BML_Context ctx,
                                            BML_RpcId rpc_id,
                                            BML_RpcInfo *out_info);

/**
 * @brief Get the name of an RPC by ID.
 */
typedef BML_Result (*PFN_BML_ImcGetRpcName)(BML_Context ctx,
                                            BML_RpcId rpc_id,
                                            char *buffer,
                                            size_t cap,
                                            size_t *out_len);

/**
 * @brief Enumerate all registered RPC endpoints.
 */
typedef void (*PFN_BML_ImcEnumerateRpc)(
    BML_Context ctx,
    void (*callback)(BML_RpcId rpc_id, const char *name, BML_Bool is_streaming, void *user_data),
    void *user_data);

/* ========================================================================
 * Core APIs - RPC Middleware
 * ======================================================================== */

/**
 * @brief Add a global RPC middleware.
 *
 * @param[in] owner      Module handle (owner of middleware)
 * @param[in] middleware  Middleware callback
 * @param[in] priority   Execution priority (lower = first)
 * @param[in] user_data  Opaque pointer passed to middleware
 * @return BML_RESULT_OK on success
 */
typedef BML_Result (*PFN_BML_ImcAddRpcMiddleware)(BML_Mod owner,
                                                  BML_RpcMiddleware middleware,
                                                  int32_t priority,
                                                  void *user_data);

/**
 * @brief Remove a previously added RPC middleware.
 */
typedef BML_Result (*PFN_BML_ImcRemoveRpcMiddleware)(BML_Mod owner,
                                                     BML_RpcMiddleware middleware);

/* ========================================================================
 * Core APIs - Streaming RPC
 * ======================================================================== */

/**
 * @brief Register a streaming RPC handler.
 */
typedef BML_Result (*PFN_BML_ImcRegisterStreamingRpc)(BML_Mod owner,
                                                      BML_RpcId rpc_id,
                                                      BML_StreamingRpcHandler handler,
                                                      void *user_data);

/**
 * @brief Push a data chunk to a stream.
 */
typedef BML_Result (*PFN_BML_ImcStreamPush)(BML_RpcStream stream,
                                             const void *data, size_t size);

/**
 * @brief Complete a stream successfully.
 */
typedef BML_Result (*PFN_BML_ImcStreamComplete)(BML_RpcStream stream);

/**
 * @brief Complete a stream with an error.
 */
typedef BML_Result (*PFN_BML_ImcStreamError)(BML_RpcStream stream,
                                              BML_Result error,
                                              const char *msg);

/**
 * @brief Call a streaming RPC.
 *
 * @param[in]  owner      Module handle (owner of call)
 * @param[in]  rpc_id     RPC ID
 * @param[in]  request    Request message
 * @param[in]  on_chunk   Handler called for each streamed chunk
 * @param[in]  on_done    Called when stream completes
 * @param[in]  user_data  Opaque pointer for callbacks
 * @param[out] out_future Future handle for overall completion
 */
typedef BML_Result (*PFN_BML_ImcCallStreamingRpc)(BML_Mod owner,
                                                  BML_RpcId rpc_id,
                                                  const BML_ImcMessage *request,
                                                  BML_ImcHandler on_chunk,
                                                  BML_FutureCallback on_done,
                                                  void *user_data,
                                                  BML_Future *out_future);

/* ========================================================================
 * RPC Interface Vtable
 * ======================================================================== */

typedef struct BML_ImcRpcInterface {
    BML_InterfaceHeader header;
    BML_Context Context;
    PFN_BML_ImcGetRpcId GetRpcId;
    PFN_BML_ImcRegisterRpc RegisterRpc;
    PFN_BML_ImcUnregisterRpc UnregisterRpc;
    PFN_BML_ImcCallRpc CallRpc;
    PFN_BML_ImcFutureAwait FutureAwait;
    PFN_BML_ImcFutureGetResult FutureGetResult;
    PFN_BML_ImcFutureGetState FutureGetState;
    PFN_BML_ImcFutureCancel FutureCancel;
    PFN_BML_ImcFutureOnComplete FutureOnComplete;
    PFN_BML_ImcFutureRelease FutureRelease;
    PFN_BML_ImcRegisterRpcEx RegisterRpcEx;
    PFN_BML_ImcCallRpcEx CallRpcEx;
    PFN_BML_ImcFutureGetError FutureGetError;
    PFN_BML_ImcGetRpcInfo GetRpcInfo;
    PFN_BML_ImcGetRpcName GetRpcName;
    PFN_BML_ImcEnumerateRpc EnumerateRpc;
    PFN_BML_ImcAddRpcMiddleware AddRpcMiddleware;
    PFN_BML_ImcRemoveRpcMiddleware RemoveRpcMiddleware;
    PFN_BML_ImcRegisterStreamingRpc RegisterStreamingRpc;
    PFN_BML_ImcStreamPush StreamPush;
    PFN_BML_ImcStreamComplete StreamComplete;
    PFN_BML_ImcStreamError StreamError;
    PFN_BML_ImcCallStreamingRpc CallStreamingRpc;
} BML_ImcRpcInterface;

BML_END_CDECLS

#endif /* BML_IMC_RPC_H */
