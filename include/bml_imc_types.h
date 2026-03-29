#ifndef BML_IMC_TYPES_H
#define BML_IMC_TYPES_H

#include "bml_types.h"
#include "bml_errors.h"
#include "bml_interface.h"
#include "bml_version.h"

/**
 * @file bml_imc_types.h
 * @brief Shared types for BML Inter-Module Communication (Bus and RPC)
 *
 * Contains all type definitions, enumerations, structures, and callback
 * signatures shared by the IMC pub/sub bus and RPC subsystems.
 * For the bus API, include bml_imc_bus.h; for the RPC API, include
 * bml_imc_rpc.h; or include bml_imc.h for the umbrella header.
 */

BML_BEGIN_CDECLS

#define BML_IMC_BUS_INTERFACE_ID "bml.imc.bus"
#define BML_IMC_RPC_INTERFACE_ID "bml.imc.rpc"

/* ========================================================================
 * ID Types
 * ======================================================================== */

/**
 * @brief 32-bit topic identifier for pub/sub messaging.
 *
 * Obtain via the runtime-owned IMC bus GetTopicId callback. IDs are
 * stable within a session but
 * may differ between runs. Cache IDs at initialization for best performance.
 */
typedef uint32_t BML_TopicId;

/**
 * @brief 32-bit RPC name identifier.
 *
 * Obtain via the runtime-owned IMC RPC GetRpcId callback. Same
 * caching advice as BML_TopicId.
 */
typedef uint32_t BML_RpcId;

/** @brief Invalid topic ID sentinel */
#define BML_TOPIC_ID_INVALID 0

/** @brief Invalid RPC ID sentinel */
#define BML_RPC_ID_INVALID 0

/* ========================================================================
 * Priority Levels
 * ======================================================================== */

typedef enum BML_ImcPriority {
    BML_IMC_PRIORITY_LOW    = 0,  /**< Low priority, processed last */
    BML_IMC_PRIORITY_NORMAL = 1,  /**< Normal priority (default) */
    BML_IMC_PRIORITY_HIGH   = 2,  /**< High priority, processed first */
    BML_IMC_PRIORITY_URGENT = 3,  /**< Urgent, bypass normal queuing */
    _BML_IMC_PRIORITY_FORCE_32BIT = 0x7FFFFFFF
} BML_ImcPriority;

/* ========================================================================
 * Event Interception
 * ======================================================================== */

/**
 * @brief Result returned by intercept handlers.
 */
typedef enum BML_EventResult {
    BML_EVENT_CONTINUE = 0,   /**< Pass to next subscriber */
    BML_EVENT_HANDLED  = 1,   /**< Stop propagation, event considered handled */
    BML_EVENT_CANCEL   = 2,   /**< Stop propagation, event considered cancelled */
    _BML_EVENT_RESULT_FORCE_32BIT = 0x7FFFFFFF
} BML_EventResult;

/* ========================================================================
 * Message Flags
 * ======================================================================== */

#define BML_IMC_FLAG_NONE         0x00000000  /**< No flags */
#define BML_IMC_FLAG_NO_COPY      0x00000001  /**< Data is zero-copy (do not free) */
#define BML_IMC_FLAG_BROADCAST    0x00000002  /**< Reserved -- not yet implemented */
#define BML_IMC_FLAG_RELIABLE     0x00000004  /**< Reserved -- not yet implemented */
#define BML_IMC_FLAG_ORDERED      0x00000008  /**< Reserved -- not yet implemented */
#define BML_IMC_FLAG_COMPRESSED   0x00000010  /**< Reserved -- not yet implemented */

/* ========================================================================
 * Backpressure Policy
 * ======================================================================== */

typedef enum BML_BackpressurePolicy {
    BML_BACKPRESSURE_DROP_OLDEST = 0,  /**< Drop oldest message when full */
    BML_BACKPRESSURE_DROP_NEWEST = 1,  /**< Drop incoming message when full */
    BML_BACKPRESSURE_BLOCK       = 2,  /**< Block publisher until space available */
    BML_BACKPRESSURE_FAIL        = 3,  /**< Return error when full */
    _BML_BACKPRESSURE_FORCE_32BIT = 0x7FFFFFFF
} BML_BackpressurePolicy;

/* ========================================================================
 * Message Types
 * ======================================================================== */

/**
 * @brief Message metadata for pub/sub and RPC.
 */
/** @brief Payload type ID for type-safe messaging. 0 = untyped/void. */
typedef uint32_t BML_PayloadTypeId;

/** @brief Sentinel for void/empty payloads or untyped messages */
#define BML_PAYLOAD_TYPE_NONE ((BML_PayloadTypeId)0)

typedef struct BML_ImcMessage {
    size_t struct_size;       /**< sizeof(BML_ImcMessage), must be first */
    const void *data;         /**< Payload data pointer */
    size_t size;              /**< Payload size in bytes */
    uint64_t msg_id;          /**< Unique message ID (0 = auto-assign) */
    uint32_t flags;           /**< Message flags (BML_IMC_FLAG_*) */
    uint32_t priority;        /**< Message priority (BML_ImcPriority) */
    uint64_t timestamp;       /**< Message timestamp (0 = auto-assign) */
    BML_TopicId reply_topic;  /**< Reserved for future request/response patterns (pass-through only) */
    BML_PayloadTypeId payload_type_id; /**< Compile-time type hash of the payload (0 = untyped) */
} BML_ImcMessage;

/** @brief Static initializer for BML_ImcMessage */
#define BML_IMC_MESSAGE_INIT { sizeof(BML_ImcMessage), NULL, 0, 0, 0, BML_IMC_PRIORITY_NORMAL, 0, 0, BML_PAYLOAD_TYPE_NONE }

/** @brief Quick message initializer macro */
#define BML_IMC_MSG(ptr, len) { sizeof(BML_ImcMessage), (ptr), (len), 0, 0, BML_IMC_PRIORITY_NORMAL, 0, 0, BML_PAYLOAD_TYPE_NONE }

/**
 * @brief Zero-copy buffer with optional cleanup callback.
 *
 * Use for large payloads or when you want to avoid copying.
 * The cleanup callback is invoked when all subscribers have processed the message.
 */
typedef struct BML_ImcBuffer {
    size_t struct_size;       /**< sizeof(BML_ImcBuffer), must be first */
    const void *data;         /**< Data pointer */
    size_t size;              /**< Data size in bytes */
    void (*cleanup)(const void *data, size_t size, void *user_data);  /**< Cleanup fn */
    void *cleanup_user_data;  /**< User data for cleanup */
} BML_ImcBuffer;

/** @brief Static initializer for BML_ImcBuffer */
#define BML_IMC_BUFFER_INIT { sizeof(BML_ImcBuffer), NULL, 0, NULL, NULL }

/* ========================================================================
 * Subscription Options
 * ======================================================================== */

/**
 * @brief Message filter callback.
 *
 * @param[in] msg      Message to filter
 * @param[in] user_data User-provided context
 * @return BML_TRUE to accept message, BML_FALSE to reject
 */
typedef BML_Bool (*BML_ImcFilter)(const BML_ImcMessage *msg, void *user_data);

/**
 * @brief Extended subscription options.
 */
typedef struct BML_SubscribeOptions {
    size_t struct_size;                /**< sizeof(BML_SubscribeOptions) */
    uint32_t queue_capacity;           /**< Queue capacity (0 = default 256) */
    BML_BackpressurePolicy backpressure; /**< What to do when queue is full */
    BML_ImcFilter filter;              /**< Optional message filter */
    void *filter_user_data;            /**< User data for filter */
    uint32_t min_priority;             /**< Minimum priority to accept */
    uint32_t flags;                    /**< Subscription flags (BML_IMC_SUBSCRIBE_FLAG_*) */
    int32_t execution_order;           /**< Subscriber execution order (lower = first, default 0) */
} BML_SubscribeOptions;

#define BML_SUBSCRIBE_OPTIONS_INIT { sizeof(BML_SubscribeOptions), 0, BML_BACKPRESSURE_DROP_OLDEST, NULL, NULL, 0, 0, 0 }

/* ========================================================================
 * Statistics Structures
 * ======================================================================== */

/**
 * @brief Per-subscription statistics.
 */
typedef struct BML_SubscriptionStats {
    size_t struct_size;           /**< sizeof(BML_SubscriptionStats) */
    uint64_t messages_received;   /**< Total messages received */
    uint64_t messages_processed;  /**< Messages successfully processed */
    uint64_t messages_dropped;    /**< Messages dropped due to backpressure */
    uint64_t total_bytes;         /**< Total bytes received */
    size_t queue_size;            /**< Current queue depth */
    size_t queue_capacity;        /**< Total queue capacity */
    uint64_t last_message_time;   /**< Timestamp of last received message */
} BML_SubscriptionStats;

#define BML_SUBSCRIPTION_STATS_INIT { sizeof(BML_SubscriptionStats), 0, 0, 0, 0, 0, 0, 0 }

/**
 * @brief Global IMC bus statistics.
 */
typedef struct BML_ImcStats {
    size_t struct_size;              /**< sizeof(BML_ImcStats) */
    uint64_t total_messages_published;  /**< Total messages published */
    uint64_t total_messages_delivered;  /**< Messages delivered to subscribers */
    uint64_t total_messages_dropped;    /**< Messages dropped across all subs */
    uint64_t total_bytes_published;     /**< Total bytes published */
    uint64_t total_rpc_calls;           /**< Total RPC calls made */
    uint64_t total_rpc_completions;     /**< RPC calls completed successfully */
    uint64_t total_rpc_failures;        /**< RPC calls that failed */
    size_t active_subscriptions;        /**< Number of active subscriptions */
    size_t active_topics;               /**< Number of active topics */
    size_t active_rpc_handlers;         /**< Number of registered RPC handlers */
    uint64_t uptime_ns;                 /**< IMC uptime in nanoseconds */
} BML_ImcStats;

#define BML_IMC_STATS_INIT { sizeof(BML_ImcStats), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }

/**
 * @brief Topic information for debugging.
 */
typedef struct BML_TopicInfo {
    size_t struct_size;           /**< sizeof(BML_TopicInfo) */
    BML_TopicId topic_id;         /**< Topic ID */
    char name[256];               /**< Topic name (empty if not tracked) */
    size_t subscriber_count;      /**< Number of subscribers */
    uint64_t message_count;       /**< Total messages published to this topic */
    BML_PayloadTypeId expected_type_id; /**< Expected payload type hash (0 = any) */
} BML_TopicInfo;

#define BML_TOPIC_INFO_INIT { sizeof(BML_TopicInfo), 0, {0}, 0, 0, BML_PAYLOAD_TYPE_NONE }

/* ========================================================================
 * Callback Types
 * ======================================================================== */

/**
 * @brief Pub/Sub message handler.
 *
 * @param[in] ctx     BML context
 * @param[in] topic   Topic ID that triggered this callback
 * @param[in] message Message data and metadata
 * @param[in] user_data User-provided context
 *
 * @threadsafe May be called from any thread
 */
typedef void (*BML_ImcHandler)(BML_Context ctx,
                               BML_TopicId topic,
                               const BML_ImcMessage *message,
                               void *user_data);

/**
 * @brief Interceptable event handler.
 *
 * Unlike BML_ImcHandler, this returns BML_EventResult and receives a
 * non-const message pointer so the handler can modify message metadata
 * (flags, priority, size) and redirect the data pointer. The data
 * pointer itself is `const void *`; to substitute different payload
 * content, point `message->data` at a handler-owned buffer.
 *
 * @param[in]     ctx       BML context
 * @param[in]     topic     Topic ID
 * @param[in,out] message   Mutable message struct
 * @param[in]     user_data User-provided context
 * @return BML_EVENT_CONTINUE to pass to next, HANDLED/CANCEL to stop
 */
typedef BML_EventResult (*BML_ImcInterceptHandler)(
    BML_Context ctx,
    BML_TopicId topic,
    BML_ImcMessage *message,
    void *user_data);

/**
 * @brief RPC handler callback.
 *
 * @param[in]  ctx       BML context
 * @param[in]  rpc_id    RPC ID that triggered this call
 * @param[in]  request   Request message
 * @param[out] response  Response buffer to fill
 * @param[in]  user_data User-provided context
 * @return BML_RESULT_OK on success, error code on failure
 *
 * @threadsafe May be called from any thread
 */
typedef BML_Result (*BML_RpcHandler)(BML_Context ctx,
                                     BML_RpcId rpc_id,
                                     const BML_ImcMessage *request,
                                     BML_ImcBuffer *response,
                                     void *user_data);

/**
 * @brief Future completion callback.
 *
 * @param[in] ctx       BML context
 * @param[in] future    Completed future handle
 * @param[in] user_data User-provided context
 *
 * @threadsafe May be called from any thread
 */
typedef void (*BML_FutureCallback)(BML_Context ctx,
                                   BML_Future future,
                                   void *user_data);

/* ========================================================================
 * Future State
 * ======================================================================== */

typedef enum BML_FutureState {
    BML_FUTURE_PENDING   = 0,  /**< Operation in progress */
    BML_FUTURE_READY     = 1,  /**< Result available */
    BML_FUTURE_CANCELLED = 2,  /**< Operation was cancelled */
    BML_FUTURE_TIMEOUT   = 3,  /**< Operation timed out */
    BML_FUTURE_FAILED    = 4,  /**< Operation failed */
    _BML_FUTURE_STATE_FORCE_32BIT = 0x7FFFFFFF
} BML_FutureState;

/* ========================================================================
 * RPC Info (introspection)
 * ======================================================================== */

typedef struct BML_RpcInfo {
    size_t struct_size;
    BML_RpcId rpc_id;
    char name[256];
    BML_Bool has_handler;
    uint64_t call_count;
    uint64_t completion_count;
    uint64_t failure_count;
    uint64_t total_latency_ns;    /**< Cumulative handler latency in nanoseconds */
} BML_RpcInfo;

#define BML_RPC_INFO_INIT { sizeof(BML_RpcInfo), 0, {0}, BML_FALSE, 0, 0, 0, 0 }

/* ========================================================================
 * RPC Call Options
 * ======================================================================== */

typedef struct BML_RpcCallOptions {
    size_t struct_size;
    uint32_t timeout_ms;          /**< 0 = no timeout */
    uint32_t flags;               /**< Reserved, must be 0 */
} BML_RpcCallOptions;

#define BML_RPC_CALL_OPTIONS_INIT { sizeof(BML_RpcCallOptions), 0, 0 }

/* ========================================================================
 * RPC Stream Handle (for streaming RPC)
 * ======================================================================== */

BML_DECLARE_HANDLE(BML_RpcStream);

/* ========================================================================
 * Extended RPC Callback Types
 * ======================================================================== */

/**
 * @brief Extended RPC handler that can return a structured error message.
 *
 * @param[in]  ctx              BML context
 * @param[in]  rpc_id           RPC ID
 * @param[in]  request          Request message
 * @param[out] response         Response buffer to fill
 * @param[out] out_error_msg    Buffer for error message (filled on failure)
 * @param[in]  error_msg_capacity Size of error message buffer
 * @param[in]  user_data        User-provided context
 * @return BML_RESULT_OK on success, error code on failure
 */
typedef BML_Result (*BML_RpcHandlerEx)(
    BML_Context ctx, BML_RpcId rpc_id,
    const BML_ImcMessage *request, BML_ImcBuffer *response,
    char *out_error_msg, size_t error_msg_capacity,
    void *user_data);

/**
 * @brief RPC middleware callback, runs before and/or after the handler.
 *
 * @param[in] ctx            BML context
 * @param[in] rpc_id         RPC ID
 * @param[in] is_pre         BML_TRUE if pre-handler, BML_FALSE if post-handler
 * @param[in] request        Request message
 * @param[in] response       Response buffer (NULL in pre, filled in post)
 * @param[in] handler_result Result from handler (BML_RESULT_OK in pre)
 * @param[in] user_data      User-provided context
 * @return BML_RESULT_OK to continue, error code to abort (pre only)
 */
typedef BML_Result (*BML_RpcMiddleware)(
    BML_Context ctx, BML_RpcId rpc_id, BML_Bool is_pre,
    const BML_ImcMessage *request, const BML_ImcBuffer *response,
    BML_Result handler_result, void *user_data);

/**
 * @brief Streaming RPC handler.
 *
 * @param[in] ctx       BML context
 * @param[in] rpc_id    RPC ID
 * @param[in] request   Request message
 * @param[in] stream    Stream handle to push chunks through
 * @param[in] user_data User-provided context
 * @return BML_RESULT_OK on success
 */
typedef BML_Result (*BML_StreamingRpcHandler)(
    BML_Context ctx, BML_RpcId rpc_id,
    const BML_ImcMessage *request, BML_RpcStream stream,
    void *user_data);

/* ========================================================================
 * Subscription Flags
 * ======================================================================== */

#define BML_IMC_SUBSCRIBE_FLAG_NONE 0u
#define BML_IMC_SUBSCRIBE_FLAG_DELIVER_RETAINED_ON_SUBSCRIBE (1u << 0)

/* ========================================================================
 * Retained State
 * ======================================================================== */

/**
 * @brief Metadata for retained topic state.
 */
typedef struct BML_ImcStateMeta {
    size_t struct_size;
    uint64_t timestamp;
    uint32_t flags;
    size_t size;
} BML_ImcStateMeta;

#define BML_IMC_STATE_META_INIT { sizeof(BML_ImcStateMeta), 0, 0, 0 }

BML_END_CDECLS

#endif /* BML_IMC_TYPES_H */
