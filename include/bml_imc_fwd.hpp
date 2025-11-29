/**
 * @file bml_imc_fwd.hpp
 * @brief Forward declarations and common types for BML IMC C++ API
 *
 * Include this header when you only need forward declarations to avoid
 * pulling in the full IMC headers.
 */

#ifndef BML_IMC_FWD_HPP
#define BML_IMC_FWD_HPP

#include "bml_imc.h"

#include <cstdint>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace bml {
namespace imc {

    // ========================================================================
    // Forward Declarations
    // ========================================================================

    class Topic;
    class Subscription;
    template <typename T> class Publisher;
    class Subscriber;
    class RpcClient;
    class RpcServer;
    class RpcFuture;
    class Message;
    class MessageBuilder;
    class Bus;

    // ========================================================================
    // Type Aliases
    // ========================================================================

    /** @brief Topic ID type */
    using TopicId = BML_TopicId;

    /** @brief RPC ID type */
    using RpcId = BML_RpcId;

    /** @brief Message priority level */
    using Priority = BML_ImcPriority;

    /** @brief Backpressure policy */
    using BackpressurePolicy = BML_BackpressurePolicy;

    /** @brief Future state */
    using FutureState = BML_FutureState;

    // ========================================================================
    // Callback Types (forward declarations)
    // ========================================================================

    /**
     * @brief Simple message callback (data + size only)
     *
     * This callback type does not require the Message class.
     */
    using SimpleCallback = std::function<void(const void *data, size_t size)>;

    /**
     * @brief Typed message callback
     * @tparam T Message payload type
     */
    template <typename T>
    using TypedCallback = std::function<void(const T &data)>;

    /**
     * @brief Message filter predicate (uses opaque types)
     */
    using FilterPredicate = std::function<bool(const void *data, size_t size)>;

    // Note: MessageCallback, RpcHandler, and TypedRpcHandler are defined in
    // bml_imc_message.hpp after the Message class is defined.

    // ========================================================================
    // Constants
    // ========================================================================

    /** @brief Invalid topic ID sentinel */
    constexpr TopicId InvalidTopicId = BML_TOPIC_ID_INVALID;

    /** @brief Invalid RPC ID sentinel */
    constexpr RpcId InvalidRpcId = BML_RPC_ID_INVALID;

    /** @brief Default queue capacity */
    constexpr size_t DefaultQueueCapacity = 256;

    /** @brief Infinite timeout */
    constexpr uint32_t InfiniteTimeout = 0;

    // ========================================================================
    // Priority Constants
    // ========================================================================

    namespace priority {

        constexpr Priority Low    = BML_IMC_PRIORITY_LOW;
        constexpr Priority Normal = BML_IMC_PRIORITY_NORMAL;
        constexpr Priority High   = BML_IMC_PRIORITY_HIGH;
        constexpr Priority Urgent = BML_IMC_PRIORITY_URGENT;

    } // namespace priority

    // ========================================================================
    // Flag Constants
    // ========================================================================

    namespace flags {

        constexpr uint32_t None       = BML_IMC_FLAG_NONE;
        constexpr uint32_t NoCopy     = BML_IMC_FLAG_NO_COPY;
        constexpr uint32_t Broadcast  = BML_IMC_FLAG_BROADCAST;
        constexpr uint32_t Reliable   = BML_IMC_FLAG_RELIABLE;
        constexpr uint32_t Ordered    = BML_IMC_FLAG_ORDERED;
        constexpr uint32_t Compressed = BML_IMC_FLAG_COMPRESSED;

    } // namespace flags

    // ========================================================================
    // Backpressure Policy Constants
    // ========================================================================

    namespace backpressure {

        constexpr BackpressurePolicy DropOldest = BML_BACKPRESSURE_DROP_OLDEST;
        constexpr BackpressurePolicy DropNewest = BML_BACKPRESSURE_DROP_NEWEST;
        constexpr BackpressurePolicy Block      = BML_BACKPRESSURE_BLOCK;
        constexpr BackpressurePolicy Fail       = BML_BACKPRESSURE_FAIL;

    } // namespace backpressure

} // namespace imc
} // namespace bml

#endif /* BML_IMC_FWD_HPP */
