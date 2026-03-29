/**
 * @file bml_imc_fwd.hpp
 * @brief Forward declarations and common types for BML IMC C++ API
 */

#ifndef BML_IMC_FWD_HPP
#define BML_IMC_FWD_HPP

#include "bml_imc.h"

#include <cstdint>
#include <cstddef>
#include <functional>
#include <string_view>

namespace bml {
namespace imc {

    namespace detail {
        template <typename MemberT>
        constexpr bool HasBusMember(const BML_ImcBusInterface *bus, size_t offset) noexcept {
            return bus != nullptr &&
                   bus->header.struct_size >= offset + sizeof(MemberT);
        }
    } // namespace detail

    // Forward Declarations
    class Topic;
    class Subscription;
    template <typename T> class Publisher;
    class RpcClient;
    class RpcServer;
    class RpcFuture;
    class RpcCallOptions;
    class RpcMiddlewareRegistration;
    class RpcStream;
    class StreamingRpcServer;
    class RpcServiceManager;
    class Message;
    class MutableMessage;
    class MessageBuilder;

    // Type Aliases
    using TopicId = BML_TopicId;
    using RpcId = BML_RpcId;
    using Priority = BML_ImcPriority;
    using BackpressurePolicy = BML_BackpressurePolicy;
    using FutureState = BML_FutureState;
    using EventResult = BML_EventResult;

    // Callback Types
    using SimpleCallback = std::function<void(const void *data, size_t size)>;

    template <typename T>
    using TypedCallback = std::function<void(const T &data)>;

    using FilterPredicate = std::function<bool(const void *data, size_t size)>;

    // Constants
    constexpr TopicId InvalidTopicId = BML_TOPIC_ID_INVALID;
    constexpr RpcId InvalidRpcId = BML_RPC_ID_INVALID;
    constexpr size_t DefaultQueueCapacity = 256;
    constexpr uint32_t InfiniteTimeout = 0;

    namespace priority {
        constexpr Priority Low    = BML_IMC_PRIORITY_LOW;
        constexpr Priority Normal = BML_IMC_PRIORITY_NORMAL;
        constexpr Priority High   = BML_IMC_PRIORITY_HIGH;
        constexpr Priority Urgent = BML_IMC_PRIORITY_URGENT;
    }

    namespace event_result {
        constexpr EventResult Continue = BML_EVENT_CONTINUE;
        constexpr EventResult Handled  = BML_EVENT_HANDLED;
        constexpr EventResult Cancel   = BML_EVENT_CANCEL;
    }

    namespace flags {
        constexpr uint32_t None       = BML_IMC_FLAG_NONE;
        constexpr uint32_t NoCopy     = BML_IMC_FLAG_NO_COPY;
        constexpr uint32_t Broadcast  = BML_IMC_FLAG_BROADCAST;
        constexpr uint32_t Reliable   = BML_IMC_FLAG_RELIABLE;
        constexpr uint32_t Ordered    = BML_IMC_FLAG_ORDERED;
        constexpr uint32_t Compressed = BML_IMC_FLAG_COMPRESSED;
    }

    namespace backpressure {
        constexpr BackpressurePolicy DropOldest = BML_BACKPRESSURE_DROP_OLDEST;
        constexpr BackpressurePolicy DropNewest = BML_BACKPRESSURE_DROP_NEWEST;
        constexpr BackpressurePolicy Block      = BML_BACKPRESSURE_BLOCK;
        constexpr BackpressurePolicy Fail       = BML_BACKPRESSURE_FAIL;
    }

    // ========================================================================
    // Compile-time type identity (needed by Message::As<T>)
    // ========================================================================

    namespace detail {
        constexpr uint32_t kFnv1aOffsetBasis = 2166136261u;
        constexpr uint32_t kFnv1aPrime = 16777619u;

        constexpr uint32_t fnv1a(const char *s, uint32_t h = kFnv1aOffsetBasis) {
            return *s ? fnv1a(s + 1, (h ^ static_cast<uint32_t>(*s)) * kFnv1aPrime) : h;
        }

        // __FUNCSIG__ is only available inside function bodies on MSVC.
        // Wrapping it in a constexpr function allows its use in static
        // member initializers via PayloadType<T>.
        template <typename T>
        constexpr uint32_t payload_type_hash() {
            return fnv1a(__FUNCSIG__);
        }

        template <typename T>
        constexpr const char *payload_type_name() {
            return __FUNCSIG__;
        }
    } // namespace detail

    template <typename T>
    struct PayloadType {
        static constexpr uint32_t Id = detail::payload_type_hash<T>();
        static constexpr const char *Name = detail::payload_type_name<T>();
    };

    template <>
    struct PayloadType<void> {
        static constexpr uint32_t Id = BML_PAYLOAD_TYPE_NONE;
        static constexpr const char *Name = "void";
    };

} // namespace imc
} // namespace bml

#endif /* BML_IMC_FWD_HPP */
