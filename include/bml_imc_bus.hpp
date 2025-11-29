/**
 * @file bml_imc_bus.hpp
 * @brief High-level IMC Bus facade and diagnostics
 *
 * Provides a unified interface to the IMC system with diagnostics support.
 */

#ifndef BML_IMC_BUS_HPP
#define BML_IMC_BUS_HPP

#include "bml_imc_fwd.hpp"
#include "bml_imc_topic.hpp"
#include "bml_imc_subscription.hpp"
#include "bml_imc_publisher.hpp"
#include "bml_imc_rpc.hpp"

#include <optional>
#include <string>
#include <sstream>

namespace bml {
namespace imc {

    // ========================================================================
    // IMC Capabilities
    // ========================================================================

    /**
     * @brief Query IMC subsystem capabilities
     */
    class Capabilities {
    public:
        /**
         * @brief Get current capabilities
         */
        static std::optional<BML_ImcCaps> Get() {
            if (!bmlImcGetCaps) return std::nullopt;
            BML_ImcCaps caps = BML_IMC_CAPS_INIT;
            if (bmlImcGetCaps(&caps) == BML_RESULT_OK) {
                return caps;
            }
            return std::nullopt;
        }

        /**
         * @brief Check if a capability is available
         */
        static bool Has(BML_ImcCapFlags flag) {
            auto caps = Get();
            return caps && (caps->capability_flags & flag);
        }

        // Convenience checks
        static bool HasPubSub() { return Has(BML_IMC_CAP_PUBSUB); }
        static bool HasRpc() { return Has(BML_IMC_CAP_RPC); }
        static bool HasFutures() { return Has(BML_IMC_CAP_FUTURES); }
        static bool HasZeroCopy() { return Has(BML_IMC_CAP_ZERO_COPY); }
        static bool HasPriority() { return Has(BML_IMC_CAP_PRIORITY); }
        static bool HasFiltering() { return Has(BML_IMC_CAP_FILTERING); }
        static bool HasStatistics() { return Has(BML_IMC_CAP_STATISTICS); }
        static bool HasBatch() { return Has(BML_IMC_CAP_BATCH); }

        /**
         * @brief Get maximum queue depth
         */
        static uint32_t MaxQueueDepth() {
            auto caps = Get();
            return caps ? caps->max_queue_depth : 256;
        }

        /**
         * @brief Get inline payload max size
         */
        static uint32_t InlinePayloadMax() {
            auto caps = Get();
            return caps ? caps->inline_payload_max : 0;
        }
    };

    // ========================================================================
    // IMC Statistics
    // ========================================================================

    /**
     * @brief Global IMC statistics access
     */
    class Statistics {
    public:
        /**
         * @brief Get current global statistics
         */
        static std::optional<BML_ImcStats> Get() {
            if (!bmlImcGetStats) return std::nullopt;
            BML_ImcStats stats = BML_IMC_STATS_INIT;
            if (bmlImcGetStats(&stats) == BML_RESULT_OK) {
                return stats;
            }
            return std::nullopt;
        }

        /**
         * @brief Reset statistics counters
         */
        static bool Reset() {
            if (!bmlImcResetStats) return false;
            return bmlImcResetStats() == BML_RESULT_OK;
        }

        // Convenience accessors
        static uint64_t MessagesPublished() {
            auto s = Get();
            return s ? s->total_messages_published : 0;
        }

        static uint64_t MessagesDelivered() {
            auto s = Get();
            return s ? s->total_messages_delivered : 0;
        }

        static uint64_t MessagesDropped() {
            auto s = Get();
            return s ? s->total_messages_dropped : 0;
        }

        static size_t ActiveSubscriptions() {
            auto s = Get();
            return s ? s->active_subscriptions : 0;
        }

        static size_t ActiveTopics() {
            auto s = Get();
            return s ? s->active_topics : 0;
        }

        static size_t ActiveRpcHandlers() {
            auto s = Get();
            return s ? s->active_rpc_handlers : 0;
        }

        /**
         * @brief Format statistics as string for logging/debugging
         */
        static std::string Format() {
            auto s = Get();
            if (!s) return "IMC Statistics: unavailable";

            std::ostringstream oss;
            oss << "IMC Statistics:\n"
                << "  Published: " << s->total_messages_published << "\n"
                << "  Delivered: " << s->total_messages_delivered << "\n"
                << "  Dropped: " << s->total_messages_dropped << "\n"
                << "  Bytes: " << s->total_bytes_published << "\n"
                << "  RPC calls: " << s->total_rpc_calls << "\n"
                << "  RPC completions: " << s->total_rpc_completions << "\n"
                << "  RPC failures: " << s->total_rpc_failures << "\n"
                << "  Active subs: " << s->active_subscriptions << "\n"
                << "  Active topics: " << s->active_topics << "\n"
                << "  Active RPC handlers: " << s->active_rpc_handlers;
            return oss.str();
        }
    };

    // ========================================================================
    // Message Pump
    // ========================================================================

    /**
     * @brief Message pump control
     *
     * Call Pump() regularly (e.g., once per frame) to process queued messages.
     */
    class Pump {
    public:
        /**
         * @brief Process pending messages
         * @param maxPerSubscription Max messages per subscription (0 = all)
         */
        static void Process(size_t maxPerSubscription = 0) {
            if (bmlImcPump) {
                bmlImcPump(maxPerSubscription);
            }
        }

        /**
         * @brief Process all pending messages
         */
        static void ProcessAll() {
            Process(0);
        }

        /**
         * @brief Process limited messages (for frame-budget scenarios)
         */
        static void ProcessBudgeted(size_t budget) {
            Process(budget);
        }
    };

    // ========================================================================
    // IMC Bus Facade
    // ========================================================================

    /**
     * @brief High-level facade for IMC operations
     *
     * Provides a convenient entry point for all IMC functionality.
     *
     * @code
     * using Bus = bml::imc::Bus;
     *
     * // Quick publish
     * Bus::publish("Events/Update", &data, sizeof(data));
     *
     * // Create typed publisher
     * auto publisher = Bus::publisher<PhysicsEvent>("Physics/Events");
     * publisher.Publish(event);
     *
     * // Subscribe
     * auto sub = Bus::subscribe("Events/Update", [](const Message& msg) {
     *     // Handle message
     * });
     *
     * // RPC
     * auto client = Bus::rpcClient("MyMod/GetHealth");
     * auto health = client.callSync<int, int>(playerId, 1000);
     *
     * // Pump messages each frame
     * Bus::pump();
     * @endcode
     */
    class Bus {
    public:
        // ====================================================================
        // Publishing
        // ====================================================================

        /**
         * @brief Quick publish to topic
         */
        static bool Publish(std::string_view topicName, const void *data = nullptr, size_t size = 0) {
            return GetTopic(topicName).Publish(data, size);
        }

        /**
         * @brief Quick publish typed data
         */
        template <typename T>
        static bool Publish(std::string_view topicName, const T &data) {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            return GetTopic(topicName).Publish(data);
        }

        /**
         * @brief Quick publish string
         */
        static bool PublishString(std::string_view topicName, std::string_view str) {
            return GetTopic(topicName).PublishString(str);
        }

        /**
         * @brief Create a typed publisher
         */
        template <typename T = void>
        static Publisher<T> CreatePublisher(std::string_view topicName) {
            return Publisher<T>(topicName);
        }

        /**
         * @brief Create a multi-publisher
         */
        static MultiPublisher CreateMultiPublisher(std::initializer_list<std::string_view> topicNames) {
            return MultiPublisher(topicNames);
        }

        // ====================================================================
        // Subscribing
        // ====================================================================

        /**
         * @brief Subscribe to topic with Message callback
         */
        static std::optional<Subscription> Subscribe(
            std::string_view topicName,
            MessageCallback callback,
            const SubscribeOptions *options = nullptr
        ) {
            return Subscription::create(topicName, std::move(callback), options);
        }

        /**
         * @brief Subscribe with simple callback (data + size)
         */
        static std::optional<Subscription> SubscribeSimple(
            std::string_view topicName,
            SimpleCallback callback,
            const SubscribeOptions *options = nullptr
        ) {
            return Subscription::createSimple(topicName, std::move(callback), options);
        }

        /**
         * @brief Subscribe with typed callback
         */
        template <typename T>
        static std::optional<Subscription> SubscribeTyped(
            std::string_view topicName,
            TypedCallback<T> callback,
            const SubscribeOptions *options = nullptr
        ) {
            return Subscription::createTyped<T>(topicName, std::move(callback), options);
        }

        /**
         * @brief Create a subscription manager
         */
        static SubscriptionManager CreateSubscriptionManager() {
            return SubscriptionManager();
        }

        // ====================================================================
        // RPC
        // ====================================================================

        /**
         * @brief Create an RPC client
         */
        static RpcClient CreateRpcClient(std::string_view name) {
            return RpcClient(name);
        }

        /**
         * @brief Create an RPC server (handler)
         */
        static RpcServer CreateRpcServer(std::string_view name, RpcHandler handler) {
            return RpcServer(name, std::move(handler));
        }

        /**
         * @brief Create a typed RPC server
         */
        template <typename Req, typename Resp>
        static RpcServer CreateRpcServer(std::string_view name, TypedRpcHandler<Req, Resp> handler) {
            return RpcServer(name, std::move(handler));
        }

        // ====================================================================
        // Topics
        // ====================================================================

        /**
         * @brief Get or create a topic
         */
        static Topic GetTopic(std::string_view name) {
            return Topic(name);
        }

        /**
         * @brief Get topic info
         */
        static std::optional<BML_TopicInfo> GetTopicInfo(TopicId id) {
            if (!bmlImcGetTopicInfo) return std::nullopt;
            BML_TopicInfo info = BML_TOPIC_INFO_INIT;
            if (bmlImcGetTopicInfo(id, &info) == BML_RESULT_OK) {
                return info;
            }
            return std::nullopt;
        }

        /**
         * @brief Get topic name by ID
         */
        static std::optional<std::string> GetTopicName(TopicId id) {
            if (!bmlImcGetTopicName) return std::nullopt;
            char buffer[256];
            size_t length = 0;
            if (bmlImcGetTopicName(id, buffer, sizeof(buffer), &length) == BML_RESULT_OK) {
                return std::string(buffer, length);
            }
            return std::nullopt;
        }

        /**
         * @brief Get the global topic registry
         */
        static TopicRegistry &GetTopicRegistry() {
            return GlobalTopicRegistry();
        }

        // ====================================================================
        // Control
        // ====================================================================

        /**
         * @brief Process pending messages
         */
        static void Pump(size_t maxPerSub = 0) {
            Pump::Process(maxPerSub);
        }

        /**
         * @brief Process all pending messages
         */
        static void PumpAll() {
            Pump::ProcessAll();
        }

        // ====================================================================
        // Diagnostics
        // ====================================================================

        /**
         * @brief Get capabilities
         */
        static std::optional<BML_ImcCaps> GetCapabilities() {
            return Capabilities::Get();
        }

        /**
         * @brief Check capability
         */
        static bool HasCapability(BML_ImcCapFlags flag) {
            return Capabilities::Has(flag);
        }

        /**
         * @brief Get statistics
         */
        static std::optional<BML_ImcStats> GetStatistics() {
            return Statistics::Get();
        }

        /**
         * @brief Reset statistics
         */
        static bool ResetStatistics() {
            return Statistics::Reset();
        }

        /**
         * @brief Format statistics as string
         */
        static std::string FormatStatistics() {
            return Statistics::Format();
        }
    };

} // namespace imc
} // namespace bml

#endif /* BML_IMC_BUS_HPP */

