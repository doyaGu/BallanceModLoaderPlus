#ifndef BML_IMC_H
#define BML_IMC_H

#include "bml_types.h"
#include "bml_errors.h"
#include "bml_version.h"

/**
 * @file bml_imc.h
 * @brief Inter-Module Communication (IMC) API
 * 
 * Provides high-performance pub/sub messaging and RPC between modules.
 * All APIs use integer IDs for maximum performance (~3-5x faster than strings).
 * 
 * @section imc_design Design Principles
 * - **ID-based only**: All publish/subscribe/RPC use integer IDs
 * - **Zero-copy**: BML_ImcBuffer enables zero-copy message passing
 * - **Thread-safe**: All APIs are thread-safe with lock-free fast paths
 * - **Robust**: Priority queues, backpressure handling, diagnostics
 * - **High-performance**: Lock-free MPSC queues, memory pools, batch operations
 * 
 * @section imc_features Features
 * - Pub/Sub with priority support
 * - Request/Response RPC with async futures
 * - Zero-copy buffer passing
 * - Per-subscription message filtering
 * - Comprehensive statistics and diagnostics
 * - Configurable queue depths and backpressure policies
 * 
 * @section imc_workflow Typical Workflow
 * @code
 * // 1. Get topic ID (one-time at init)
 * BML_TopicId topic;
 * bmlImcGetTopicId("MyMod/Events/Update", &topic);
 * 
 * // 2. Subscribe with options
 * BML_SubscribeOptions opts = BML_SUBSCRIBE_OPTIONS_INIT;
 * opts.queue_capacity = 512;
 * BML_Subscription sub;
 * bmlImcSubscribeEx(topic, my_handler, user_data, &opts, &sub);
 * 
 * // 3. Publish with priority
 * BML_ImcMessage msg = BML_IMC_MSG(&my_data, sizeof(my_data));
 * msg.priority = BML_IMC_PRIORITY_HIGH;
 * bmlImcPublish(topic, my_data, sizeof(my_data), &msg);
 * 
 * // 4. Cleanup
 * bmlImcUnsubscribe(sub);
 * @endcode
 */

BML_BEGIN_CDECLS

/* ========================================================================
 * ID Types
 * ======================================================================== */

/**
 * @brief 32-bit topic identifier for pub/sub messaging.
 * 
 * Obtain via bmlImcGetTopicId(). IDs are stable within a session but
 * may differ between runs. Cache IDs at initialization for best performance.
 */
typedef uint32_t BML_TopicId;

/**
 * @brief 32-bit RPC name identifier.
 * 
 * Obtain via bmlImcGetRpcId(). Same caching advice as BML_TopicId.
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
 * Message Flags
 * ======================================================================== */

#define BML_IMC_FLAG_NONE         0x00000000  /**< No flags */
#define BML_IMC_FLAG_NO_COPY      0x00000001  /**< Data is zero-copy (do not free) */
#define BML_IMC_FLAG_BROADCAST    0x00000002  /**< Broadcast to all subscribers */
#define BML_IMC_FLAG_RELIABLE     0x00000004  /**< Reliable delivery (retry on failure) */
#define BML_IMC_FLAG_ORDERED      0x00000008  /**< Preserve ordering per-sender */
#define BML_IMC_FLAG_COMPRESSED   0x00000010  /**< Payload is compressed */

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
typedef struct BML_ImcMessage {
    size_t struct_size;       /**< sizeof(BML_ImcMessage), must be first */
    const void *data;         /**< Payload data pointer */
    size_t size;              /**< Payload size in bytes */
    uint64_t msg_id;          /**< Unique message ID (0 = auto-assign) */
    uint32_t flags;           /**< Message flags (BML_IMC_FLAG_*) */
    uint32_t priority;        /**< Message priority (BML_ImcPriority) */
    uint64_t timestamp;       /**< Message timestamp (0 = auto-assign) */
    BML_TopicId reply_topic;  /**< Reply topic for request/response patterns */
} BML_ImcMessage;

/** @brief Static initializer for BML_ImcMessage */
#define BML_IMC_MESSAGE_INIT { sizeof(BML_ImcMessage), NULL, 0, 0, 0, BML_IMC_PRIORITY_NORMAL, 0, 0 }

/** @brief Quick message initializer macro */
#define BML_IMC_MSG(ptr, len) { sizeof(BML_ImcMessage), (ptr), (len), 0, 0, BML_IMC_PRIORITY_NORMAL, 0, 0 }

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
} BML_SubscribeOptions;

#define BML_SUBSCRIBE_OPTIONS_INIT { sizeof(BML_SubscribeOptions), 0, BML_BACKPRESSURE_DROP_OLDEST, NULL, NULL, 0 }

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
} BML_TopicInfo;

#define BML_TOPIC_INFO_INIT { sizeof(BML_TopicInfo), 0, {0}, 0, 0 }

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
 * @brief RPC handler callback.
 * 
 * @param[in]  ctx       BML context
 * @param[in]  rpc_id    RPC ID that triggered this call
 * @param[in]  request   Request message
 * @param[out] response  Response buffer to fill
 * @param[in]  user_data User-provided context
 * @return BML_OK on success, error code on failure
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
 * Core APIs - Topic/RPC ID Resolution
 * ======================================================================== */

/**
 * @brief Get or create a topic ID from a string name.
 * 
 * First call creates the ID, subsequent calls return the same ID.
 * Thread-safe with internal synchronization.
 * 
 * @param[in]  name   Topic name (e.g., "MyMod/Events/Update")
 * @param[out] out_id Receives the topic ID
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_ARGUMENT if name or out_id is NULL
 * 
 * @threadsafe Yes
 * 
 * @code
 * BML_TopicId topic;
 * bmlImcGetTopicId("Physics/Tick", &topic);
 * // Cache 'topic' and use for all publish/subscribe calls
 * @endcode
 */
typedef BML_Result (*PFN_BML_ImcGetTopicId)(const char *name, BML_TopicId *out_id);

/**
 * @brief Get or create an RPC ID from a string name.
 * 
 * @param[in]  name   RPC name (e.g., "MyMod/GetHealth")
 * @param[out] out_id Receives the RPC ID
 * @return BML_OK on success
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcGetRpcId)(const char *name, BML_RpcId *out_id);

/* ========================================================================
 * Core APIs - Pub/Sub
 * ======================================================================== */

/**
 * @brief Publish a message to a topic.
 * 
 * Message is delivered to all active subscribers. Payload is copied unless
 * using zero-copy buffer variant.
 * 
 * @param[in] topic   Topic ID from bmlImcGetTopicId()
 * @param[in] data    Payload data (may be NULL if size is 0)
 * @param[in] size    Payload size in bytes
 * @param[in] msg     Optional message metadata (may be NULL)
 * @return BML_RESULT_OK on success (even if no subscribers)
 * @return BML_RESULT_INVALID_ARGUMENT if topic is BML_TOPIC_ID_INVALID
 * 
 * @threadsafe Yes
 * 
 * @code
 * float delta_time = 0.016f;
 * bmlImcPublish(physics_tick_topic, &delta_time, sizeof(delta_time), NULL);
 * @endcode
 */
typedef BML_Result (*PFN_BML_ImcPublish)(BML_TopicId topic,
                                         const void *data,
                                         size_t size,
                                         const BML_ImcMessage *msg);

/**
 * @brief Publish a zero-copy buffer to a topic.
 * 
 * Use for large payloads. The cleanup callback is invoked after all
 * subscribers have processed the message.
 * 
 * @param[in] topic  Topic ID
 * @param[in] buffer Zero-copy buffer with cleanup callback
 * @return BML_RESULT_OK on success
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcPublishBuffer)(BML_TopicId topic,
                                               const BML_ImcBuffer *buffer);

/**
 * @brief Subscribe to a topic.
 * 
 * @param[in]  topic    Topic ID
 * @param[in]  handler  Callback invoked on each message
 * @param[in]  user_data Opaque pointer passed to handler
 * @param[out] out_sub  Receives subscription handle
 * @return BML_RESULT_OK on success
 * 
 * @threadsafe Yes
 * 
 * @code
 * void on_physics_tick(BML_Context ctx, BML_TopicId topic, 
 *                      const BML_ImcMessage *msg, void *ud) {
 *     float dt = *(float*)msg->data;
 *     // Process tick...
 * }
 * 
 * BML_Subscription sub;
 * bmlImcSubscribe(physics_topic, on_physics_tick, NULL, &sub);
 * @endcode
 */
typedef BML_Result (*PFN_BML_ImcSubscribe)(BML_TopicId topic,
                                           BML_ImcHandler handler,
                                           void *user_data,
                                           BML_Subscription *out_sub);

/**
 * @brief Unsubscribe and release a subscription.
 * 
 * After this call, no more callbacks will be invoked and the handle
 * becomes invalid.
 * 
 * @param[in] sub Subscription handle
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_HANDLE if sub is invalid
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcUnsubscribe)(BML_Subscription sub);

/**
 * @brief Check if a subscription is still active.
 * 
 * @param[in]  sub    Subscription handle
 * @param[out] out_active Receives BML_TRUE if active
 * @return BML_RESULT_OK on success
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcSubscriptionIsActive)(BML_Subscription sub,
                                                       BML_Bool *out_active);

/**
 * @brief Subscribe with extended options.
 * 
 * @param[in]  topic    Topic ID
 * @param[in]  handler  Callback invoked on each message
 * @param[in]  user_data Opaque pointer passed to handler
 * @param[in]  options  Extended subscription options (may be NULL for defaults)
 * @param[out] out_sub  Receives subscription handle
 * @return BML_RESULT_OK on success
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcSubscribeEx)(BML_TopicId topic,
                                             BML_ImcHandler handler,
                                             void *user_data,
                                             const BML_SubscribeOptions *options,
                                             BML_Subscription *out_sub);

/**
 * @brief Get statistics for a subscription.
 * 
 * @param[in]  sub   Subscription handle
 * @param[out] stats Receives subscription statistics
 * @return BML_RESULT_OK on success
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcGetSubscriptionStats)(BML_Subscription sub,
                                                       BML_SubscriptionStats *stats);

/**
 * @brief Publish to multiple topics atomically.
 * 
 * @param[in]  topics  Array of topic IDs
 * @param[in]  count   Number of topics
 * @param[in]  data    Payload data
 * @param[in]  size    Payload size
 * @param[in]  msg     Optional message metadata
 * @param[out] out_delivered Optional count of successful deliveries
 * @return BML_RESULT_OK on success
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcPublishMulti)(const BML_TopicId *topics,
                                               size_t count,
                                               const void *data,
                                               size_t size,
                                               const BML_ImcMessage *msg,
                                               size_t *out_delivered);

/* ========================================================================
 * Core APIs - RPC
 * ======================================================================== */

/**
 * @brief Register an RPC handler.
 * 
 * @param[in] rpc_id   RPC ID from bmlImcGetRpcId()
 * @param[in] handler  Handler callback
 * @param[in] user_data Opaque pointer passed to handler
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_ALREADY_EXISTS if handler already registered
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcRegisterRpc)(BML_RpcId rpc_id,
                                             BML_RpcHandler handler,
                                             void *user_data);

/**
 * @brief Unregister an RPC handler.
 * 
 * @param[in] rpc_id  RPC ID
 * @return BML_RESULT_OK on success
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcUnregisterRpc)(BML_RpcId rpc_id);

/**
 * @brief Call an RPC asynchronously.
 * 
 * @param[in]  rpc_id   RPC ID
 * @param[in]  request  Request message (data + size)
 * @param[out] out_future Receives future handle for result
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_NOT_FOUND if no handler registered
 * 
 * @threadsafe Yes
 * 
 * @code
 * BML_ImcMessage req = BML_IMC_MSG(&query, sizeof(query));
 * BML_Future future;
 * bmlImcCallRpc(get_health_rpc, &req, &future);
 * 
 * bmlImcFutureAwait(future, 1000);
 * 
 * BML_ImcMessage response;
 * bmlImcFutureGetResult(future, &response);
 * int health = *(int*)response.data;
 * 
 * bmlImcFutureRelease(future);
 * @endcode
 */
typedef BML_Result (*PFN_BML_ImcCallRpc)(BML_RpcId rpc_id,
                                         const BML_ImcMessage *request,
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
 * @param[out] out_message Receives result message
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
 * @param[in] future    Future handle
 * @param[in] callback  Callback to invoke on completion
 * @param[in] user_data Opaque pointer passed to callback
 * @return BML_RESULT_OK on success
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcFutureOnComplete)(BML_Future future,
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

/* ========================================================================
 * Core APIs - Message Pump
 * ======================================================================== */

/**
 * @brief Process pending messages.
 * 
 * Call periodically (e.g., once per frame) to dispatch queued messages.
 * 
 * @param[in] max_per_sub Maximum messages to process per subscription (0 = all)
 * 
 * @threadsafe Yes
 */
typedef void (*PFN_BML_ImcPump)(size_t max_per_sub);

/* ========================================================================
 * Diagnostics APIs
 * ======================================================================== */

/**
 * @brief Get global IMC statistics.
 * 
 * @param[out] stats Receives global statistics
 * @return BML_RESULT_OK on success
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcGetStats)(BML_ImcStats *stats);

/**
 * @brief Reset global statistics counters.
 * 
 * @return BML_RESULT_OK on success
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcResetStats)(void);

/**
 * @brief Get information about a topic.
 * 
 * @param[in]  topic_id Topic ID
 * @param[out] info     Receives topic information
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_NOT_FOUND if topic doesn't exist
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcGetTopicInfo)(BML_TopicId topic_id, BML_TopicInfo *info);

/**
 * @brief Lookup topic name by ID (reverse lookup).
 * 
 * @param[in]  topic_id    Topic ID
 * @param[out] name_buffer Buffer to receive name
 * @param[in]  buffer_size Size of buffer
 * @param[out] out_length  Receives actual name length (may be NULL)
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_NOT_FOUND if topic not registered
 * @return BML_RESULT_BUFFER_TOO_SMALL if buffer too small
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcGetTopicName)(BML_TopicId topic_id,
                                               char *name_buffer,
                                               size_t buffer_size,
                                               size_t *out_length);

/* ========================================================================
 * Capabilities Query
 * ======================================================================== */

typedef enum BML_ImcCapFlags {
    BML_IMC_CAP_PUBSUB       = 1u << 0,  /**< Pub/sub supported */
    BML_IMC_CAP_RPC          = 1u << 1,  /**< RPC supported */
    BML_IMC_CAP_FUTURES      = 1u << 2,  /**< Async futures supported */
    BML_IMC_CAP_ZERO_COPY    = 1u << 3,  /**< Zero-copy buffers supported */
    BML_IMC_CAP_PRIORITY     = 1u << 4,  /**< Priority queues supported */
    BML_IMC_CAP_FILTERING    = 1u << 5,  /**< Message filtering supported */
    BML_IMC_CAP_STATISTICS   = 1u << 6,  /**< Statistics collection supported */
    BML_IMC_CAP_BATCH        = 1u << 7,  /**< Batch operations supported */
    _BML_IMC_CAP_FORCE_32BIT = 0x7FFFFFFF
} BML_ImcCapFlags;

typedef struct BML_ImcCaps {
    size_t struct_size;           /**< sizeof(BML_ImcCaps), must be first */
    BML_Version api_version;      /**< API version */
    uint32_t capability_flags;    /**< Bitmask of BML_ImcCapFlags */
    uint32_t max_topic_count;     /**< Maximum topics (0 = unlimited) */
    uint32_t max_queue_depth;     /**< Default queue depth per subscription */
    uint32_t inline_payload_max;  /**< Max bytes for inline (no-alloc) payloads */
} BML_ImcCaps;

#define BML_IMC_CAPS_INIT { sizeof(BML_ImcCaps), BML_VERSION_INIT(0,0,0), 0, 0, 0, 0 }

typedef BML_Result (*PFN_BML_ImcGetCaps)(BML_ImcCaps *out_caps);

/* ========================================================================
 * Global Function Pointers
 * ======================================================================== */

/* ID Resolution */
extern PFN_BML_ImcGetTopicId         bmlImcGetTopicId;
extern PFN_BML_ImcGetRpcId           bmlImcGetRpcId;

/* Pub/Sub */
extern PFN_BML_ImcPublish            bmlImcPublish;
extern PFN_BML_ImcPublishBuffer      bmlImcPublishBuffer;
extern PFN_BML_ImcSubscribe          bmlImcSubscribe;
extern PFN_BML_ImcUnsubscribe        bmlImcUnsubscribe;
extern PFN_BML_ImcSubscriptionIsActive bmlImcSubscriptionIsActive;

/* RPC */
extern PFN_BML_ImcRegisterRpc        bmlImcRegisterRpc;
extern PFN_BML_ImcUnregisterRpc      bmlImcUnregisterRpc;
extern PFN_BML_ImcCallRpc            bmlImcCallRpc;

/* Futures */
extern PFN_BML_ImcFutureAwait        bmlImcFutureAwait;
extern PFN_BML_ImcFutureGetResult    bmlImcFutureGetResult;
extern PFN_BML_ImcFutureGetState     bmlImcFutureGetState;
extern PFN_BML_ImcFutureCancel       bmlImcFutureCancel;
extern PFN_BML_ImcFutureOnComplete   bmlImcFutureOnComplete;
extern PFN_BML_ImcFutureRelease      bmlImcFutureRelease;

/* Pump & Caps */
extern PFN_BML_ImcPump               bmlImcPump;
extern PFN_BML_ImcGetCaps            bmlImcGetCaps;

/* Extended Subscribe */
extern PFN_BML_ImcSubscribeEx        bmlImcSubscribeEx;
extern PFN_BML_ImcGetSubscriptionStats bmlImcGetSubscriptionStats;

/* Batch Operations */
extern PFN_BML_ImcPublishMulti       bmlImcPublishMulti;

/* Diagnostics */
extern PFN_BML_ImcGetStats           bmlImcGetStats;
extern PFN_BML_ImcResetStats         bmlImcResetStats;
extern PFN_BML_ImcGetTopicInfo       bmlImcGetTopicInfo;
extern PFN_BML_ImcGetTopicName       bmlImcGetTopicName;

BML_END_CDECLS

/* ========================================================================
 * Compile-Time Assertions for ABI Stability
 * ======================================================================== */

#ifdef __cplusplus
#include <cstddef>
#define BML_IMC_OFFSETOF(type, member) offsetof(type, member)
#else
#include <stddef.h>
#define BML_IMC_OFFSETOF(type, member) offsetof(type, member)
#endif

#if defined(__cplusplus) && __cplusplus >= 201103L
    #define BML_IMC_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define BML_IMC_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
    #define BML_IMC_STATIC_ASSERT_CONCAT_(a, b) a##b
    #define BML_IMC_STATIC_ASSERT_CONCAT(a, b) BML_IMC_STATIC_ASSERT_CONCAT_(a, b)
    #define BML_IMC_STATIC_ASSERT(cond, msg) \
        typedef char BML_IMC_STATIC_ASSERT_CONCAT(bml_imc_assert_, __LINE__)[(cond) ? 1 : -1]
#endif

/* Verify struct_size is at offset 0 */
BML_IMC_STATIC_ASSERT(BML_IMC_OFFSETOF(BML_ImcMessage, struct_size) == 0,
    "BML_ImcMessage.struct_size must be at offset 0");
BML_IMC_STATIC_ASSERT(BML_IMC_OFFSETOF(BML_ImcBuffer, struct_size) == 0,
    "BML_ImcBuffer.struct_size must be at offset 0");
BML_IMC_STATIC_ASSERT(BML_IMC_OFFSETOF(BML_ImcCaps, struct_size) == 0,
    "BML_ImcCaps.struct_size must be at offset 0");

/* Verify enum sizes are 32-bit */
BML_IMC_STATIC_ASSERT(sizeof(BML_FutureState) == sizeof(int32_t),
    "BML_FutureState must be 32-bit");

#endif /* BML_IMC_H */
