#ifndef BML_IMC_BUS_H
#define BML_IMC_BUS_H

#include "bml_imc_types.h"

/**
 * @file bml_imc_bus.h
 * @brief IMC Pub/Sub Bus API
 *
 * Provides the pub/sub messaging bus interface including topic resolution,
 * publish/subscribe operations, event interception, retained state,
 * message pump, and diagnostics. All function pointer types and the
 * BML_ImcBusInterface vtable are defined here.
 */

BML_BEGIN_CDECLS

/* ========================================================================
 * Core APIs - Topic/RPC ID Resolution
 * ======================================================================== */

/**
 * @brief Get or create a topic ID from a string name.
 *
 * First call creates the ID, subsequent calls return the same ID.
 * Thread-safe with internal synchronization.
 *
 * @param[in]  ctx    BML context
 * @param[in]  name   Topic name (e.g., "MyMod/Events/Update")
 * @param[out] out_id Receives the topic ID
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_ARGUMENT if name or out_id is NULL
 *
 * @threadsafe Yes
 *
 * @code
 * BML_TopicId topic;
 * getTopicId("Physics/Tick", &topic);
 * // Cache 'topic' and use for all publish/subscribe calls
 * @endcode
 */
typedef BML_Result (*PFN_BML_ImcGetTopicId)(BML_Context ctx,
                                            const char *name,
                                            BML_TopicId *out_id);

/* ========================================================================
 * Core APIs - Pub/Sub
 * ======================================================================== */

/**
 * @brief Publish a message to a topic (simple version).
 *
 * Message is delivered to all active subscribers. Payload is copied.
 * For advanced options (priority, flags, etc.), use bmlImcPublishEx.
 *
 * @param[in] owner  Module handle (owner of publish call)
 * @param[in] topic  Topic ID resolved through the IMC bus interface
 * @param[in] data   Payload data (may be NULL if size is 0)
 * @param[in] size   Payload size in bytes
 * @return BML_RESULT_OK on success (even if no subscribers)
 * @return BML_RESULT_INVALID_ARGUMENT if topic is BML_TOPIC_ID_INVALID
 *
 * @threadsafe Yes
 *
 * @code
 * float delta_time = 0.016f;
 * publish(physics_tick_topic, &delta_time, sizeof(delta_time));
 * @endcode
 */
typedef BML_Result (*PFN_BML_ImcPublish)(BML_Mod owner,
                                         BML_TopicId topic,
                                         const void *data,
                                         size_t size);

/**
 * @brief Publish a message to a topic with extended options.
 *
 * Message is delivered to all active subscribers. Payload is copied unless
 * using zero-copy buffer variant. Use this for advanced features like
 * priority, flags, or custom message metadata.
 *
 * @param[in] owner  Module handle (owner of publish call)
 * @param[in] topic  Topic ID resolved through the IMC bus interface
 * @param[in] msg    Message with payload and metadata (must not be NULL)
 * @return BML_RESULT_OK on success (even if no subscribers)
 * @return BML_RESULT_INVALID_ARGUMENT if topic is invalid or msg is NULL
 *
 * @threadsafe Yes
 *
 * @code
 * BML_ImcMessage msg = BML_IMC_MSG(&my_data, sizeof(my_data));
 * msg.priority = BML_IMC_PRIORITY_HIGH;
 * publishEx(physics_tick_topic, &msg);
 * @endcode
 */
typedef BML_Result (*PFN_BML_ImcPublishEx)(BML_Mod owner,
                                           BML_TopicId topic,
                                           const BML_ImcMessage *msg);

/**
 * @brief Publish a zero-copy buffer to a topic.
 *
 * Use for large payloads. The cleanup callback is invoked after all
 * subscribers have processed the message.
 *
 * @param[in] owner  Module handle (owner of publish call)
 * @param[in] topic  Topic ID
 * @param[in] buffer Zero-copy buffer with cleanup callback
 * @return BML_RESULT_OK on success
 *
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcPublishBuffer)(BML_Mod owner,
                                               BML_TopicId topic,
                                               const BML_ImcBuffer *buffer);

/**
 * @brief Publish the same payload to multiple topics.
 *
 * @param[in]  owner  Module handle (owner of publish call)
 * @param[in]  topics Array of topic IDs
 * @param[in]  count  Number of topics
 * @param[in]  data   Payload data
 * @param[in]  size   Payload size
 * @param[in]  msg    Optional message metadata
 * @param[out] out_delivered Optional count of successful deliveries
 * Best-effort only: delivery is attempted per topic and is not transactional.
 *
 * @return BML_RESULT_OK if at least one topic accepted the publish
 *
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcPublishMulti)(BML_Mod owner,
                                              const BML_TopicId *topics,
                                              size_t count,
                                              const void *data,
                                              size_t size,
                                              const BML_ImcMessage *msg,
                                              size_t *out_delivered);

/* ========================================================================
 * Core APIs - Event Interception
 * ======================================================================== */

/**
 * @brief Publish an interceptable event.
 *
 * Dispatches to intercept subscribers in execution_order. Stops on
 * HANDLED or CANCEL result. Then delivers to regular subscribers
 * unless the result is CANCEL.
 *
 * @param[in]  owner      Module handle (owner of publish call)
 * @param[in]  topic      Topic ID
 * @param[in]  msg        Message (non-const: intercept handlers may modify)
 * @param[out] out_result Final event disposition (may be NULL)
 * @return BML_RESULT_OK on success
 *
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcPublishInterceptable)(
    BML_Mod owner,
    BML_TopicId topic,
    BML_ImcMessage *msg,
    BML_EventResult *out_result);

/**
 * @brief Subscribe to a topic.
 *
 * @param[in]  owner    Module handle (owner of subscription)
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
 * subscribe(physics_topic, on_physics_tick, NULL, &sub);
 * @endcode
 */
typedef BML_Result (*PFN_BML_ImcSubscribe)(BML_Mod owner,
                                           BML_TopicId topic,
                                           BML_ImcHandler handler,
                                           void *user_data,
                                           BML_Subscription *out_sub);

/**
 * @brief Subscribe with extended options.
 *
 * @param[in]  owner    Module handle (owner of subscription)
 * @param[in]  topic    Topic ID
 * @param[in]  handler  Callback invoked on each message
 * @param[in]  user_data Opaque pointer passed to handler
 * @param[in]  options  Extended subscription options (may be NULL for defaults)
 * @param[out] out_sub  Receives subscription handle
 * @return BML_RESULT_OK on success
 *
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcSubscribeEx)(BML_Mod owner,
                                             BML_TopicId topic,
                                             BML_ImcHandler handler,
                                             void *user_data,
                                             const BML_SubscribeOptions *options,
                                             BML_Subscription *out_sub);

/**
 * @brief Subscribe an intercept handler to a topic.
 *
 * Intercept handlers receive a non-const message and return BML_EventResult.
 * They are dispatched in execution_order (lower first) during
 * PublishInterceptable calls. Regular Publish calls skip intercept handlers.
 *
 * @param[in]  owner    Module handle (owner of subscription)
 * @param[in]  topic    Topic ID
 * @param[in]  handler  Intercept callback
 * @param[in]  user_data Opaque pointer passed to handler
 * @param[out] out_sub  Receives subscription handle
 * @return BML_RESULT_OK on success
 *
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcSubscribeIntercept)(
    BML_Mod owner,
    BML_TopicId topic,
    BML_ImcInterceptHandler handler,
    void *user_data,
    BML_Subscription *out_sub);

/**
 * @brief Subscribe an intercept handler with extended options.
 *
 * @param[in]  owner    Module handle (owner of subscription)
 * @param[in]  topic    Topic ID
 * @param[in]  handler  Intercept callback
 * @param[in]  user_data Opaque pointer passed to handler
 * @param[in]  options  Extended subscription options (execution_order used)
 * @param[out] out_sub  Receives subscription handle
 * @return BML_RESULT_OK on success
 *
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcSubscribeInterceptEx)(
    BML_Mod owner,
    BML_TopicId topic,
    BML_ImcInterceptHandler handler,
    void *user_data,
    const BML_SubscribeOptions *options,
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

/* ========================================================================
 * Core APIs - Retained State
 * ======================================================================== */

/**
 * @brief Publish a message and retain its state on the topic.
 *
 * New subscribers with BML_IMC_SUBSCRIBE_FLAG_DELIVER_RETAINED_ON_SUBSCRIBE
 * will receive the retained state immediately.
 */
typedef BML_Result (*PFN_BML_ImcPublishState)(BML_Mod owner,
                                              BML_TopicId topic,
                                              const BML_ImcMessage *msg);

/**
 * @brief Copy the retained state for a topic into a caller-owned buffer.
 */
typedef BML_Result (*PFN_BML_ImcCopyState)(BML_Context ctx,
                                           BML_TopicId topic,
                                           void *dst,
                                           size_t dst_size,
                                           size_t *out_size,
                                           BML_ImcStateMeta *out_meta);

/**
 * @brief Clear the retained state for a topic.
 */
typedef BML_Result (*PFN_BML_ImcClearState)(BML_Context ctx, BML_TopicId topic);

/* ========================================================================
 * Core APIs - Message Pump
 * ======================================================================== */

/**
 * @brief Process pending messages.
 *
 * Call periodically (e.g., once per frame) to dispatch queued messages.
 *
 * @param[in] ctx         BML context
 * @param[in] max_per_sub Maximum messages to process per subscription (0 = all)
 *
 * @threadsafe No
 * @warning Never call from within a subscriber callback -- it will deadlock.
 */
typedef void (*PFN_BML_ImcPump)(BML_Context ctx, size_t max_per_sub);

/* ========================================================================
 * Diagnostics APIs
 * ======================================================================== */

/**
 * @brief Get global IMC statistics.
 *
 * @param[in]  ctx   BML context
 * @param[out] out_stats Receives global statistics
 * @return BML_RESULT_OK on success
 *
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcGetStats)(BML_Context ctx, BML_ImcStats *out_stats);

/**
 * @brief Reset global IMC statistics.
 *
 * @param[in] ctx BML context
 * @return BML_RESULT_OK on success
 *
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcResetStats)(BML_Context ctx);

/**
 * @brief Get information about a specific topic.
 *
 * @param[in]  ctx      BML context
 * @param[in]  topic    Topic ID
 * @param[out] out_info Receives topic information
 * @return BML_RESULT_OK on success
 *
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcGetTopicInfo)(BML_Context ctx,
                                              BML_TopicId topic,
                                              BML_TopicInfo *out_info);

/**
 * @brief Get the name of a topic by ID.
 *
 * @param[in]  ctx         BML context
 * @param[in]  topic       Topic ID
 * @param[out] buffer      Buffer to receive topic name
 * @param[in]  buffer_size Size of buffer
 * @param[out] out_length  Receives actual name length (may be NULL)
 * @return BML_RESULT_OK on success
 *
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_ImcGetTopicName)(BML_Context ctx,
                                              BML_TopicId topic,
                                              char *buffer,
                                              size_t buffer_size,
                                              size_t *out_length);

/* ========================================================================
 * Bus Interface Vtable
 * ======================================================================== */

typedef struct BML_ImcBusInterface {
    BML_InterfaceHeader header;
    BML_Context Context;
    PFN_BML_ImcGetTopicId GetTopicId;
    PFN_BML_ImcPublish Publish;
    PFN_BML_ImcPublishEx PublishEx;
    PFN_BML_ImcPublishBuffer PublishBuffer;
    PFN_BML_ImcPublishMulti PublishMulti;
    PFN_BML_ImcPublishInterceptable PublishInterceptable;
    PFN_BML_ImcSubscribe Subscribe;
    PFN_BML_ImcSubscribeEx SubscribeEx;
    PFN_BML_ImcSubscribeIntercept SubscribeIntercept;
    PFN_BML_ImcSubscribeInterceptEx SubscribeInterceptEx;
    PFN_BML_ImcUnsubscribe Unsubscribe;
    PFN_BML_ImcSubscriptionIsActive SubscriptionIsActive;
    PFN_BML_ImcPublishState PublishState;
    PFN_BML_ImcCopyState CopyState;
    PFN_BML_ImcClearState ClearState;
    PFN_BML_ImcPump Pump;
    PFN_BML_ImcGetSubscriptionStats GetSubscriptionStats;
    PFN_BML_ImcGetStats GetStats;
    PFN_BML_ImcResetStats ResetStats;
    PFN_BML_ImcGetTopicInfo GetTopicInfo;
    PFN_BML_ImcGetTopicName GetTopicName;
} BML_ImcBusInterface;

BML_END_CDECLS

#endif /* BML_IMC_BUS_H */
