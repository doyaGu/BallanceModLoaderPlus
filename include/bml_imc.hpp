/**
 * @file bml_imc.hpp
 * @brief BML C++ Inter-Mod Communication API
 *
 * Main header for the C++ IMC API. Includes all components.
 *
 * @section imc_cpp_quickstart Quick Start
 *
 * @code
 * #include <bml_imc.hpp>
 *
 * using namespace bml::imc;
 *
 * // Publishing
 * Topic topic("MyMod/Events/Update", bus);
 * topic.Publish(myData);
 *
 * // Typed publisher
 * Publisher<MyEvent> pub("MyMod/Events/Custom", bus);
 * pub.Publish(MyEvent{...});
 *
 * // Subscribing
 * auto sub = Subscription::create("OtherMod/Events",
 *     [](const Message& msg) { ... }, nullptr, bus);
 *
 * // Typed subscription
 * auto typed = Subscription::createTyped<MyEvent>("MyMod/Events/Custom",
 *     [](const MyEvent& event) { ... }, nullptr, bus);
 *
 * // Intercept
 * auto intercept = Subscription::createIntercept("Game/Event",
 *     [](MutableMessage& msg) -> EventResult {
 *         return event_result::Continue;
 *     }, nullptr, bus);
 *
 * // Message pump (call each frame)
 * bml::imc::pump(bus);
 * @endcode
 */

#ifndef BML_IMC_HPP
#define BML_IMC_HPP

#include "bml_imc_fwd.hpp"
#include "bml_imc_message.hpp"
#include "bml_imc_topic.hpp"
#include "bml_imc_subscription.hpp"
#include "bml_imc_publisher.hpp"
#include "bml_imc_rpc.hpp"

#include <optional>
#include <string>
#include <sstream>
#include <vector>

namespace bml {
namespace imc {

    // ========================================================================
    // Free functions replacing the old Bus / Pump / Statistics classes
    // ========================================================================

    /** @brief Process pending messages (call once per frame) */
    inline void pump(size_t maxPerSub = 0, const BML_ImcBusInterface *bus = nullptr) {
        if (bus && bus->Pump) bus->Pump(maxPerSub);
    }

    /** @brief Process all pending messages */
    inline void pumpAll(const BML_ImcBusInterface *bus = nullptr) {
        pump(0, bus);
    }

    /** @brief Get global IMC statistics */
    inline std::optional<BML_ImcStats> getStats(const BML_ImcBusInterface *bus = nullptr) {
        if (!bus || !bus->GetStats) return std::nullopt;
        BML_ImcStats stats = BML_IMC_STATS_INIT;
        if (bus->GetStats(&stats) == BML_RESULT_OK) return stats;
        return std::nullopt;
    }

    /** @brief Reset global statistics counters */
    inline bool resetStats(const BML_ImcBusInterface *bus = nullptr) {
        if (!bus || !bus->ResetStats) return false;
        return bus->ResetStats() == BML_RESULT_OK;
    }

    /** @brief Get topic info by ID */
    inline std::optional<BML_TopicInfo> getTopicInfo(TopicId id, const BML_ImcBusInterface *bus = nullptr) {
        if (!bus || !bus->GetTopicInfo) return std::nullopt;
        BML_TopicInfo info = BML_TOPIC_INFO_INIT;
        if (bus->GetTopicInfo(id, &info) == BML_RESULT_OK) return info;
        return std::nullopt;
    }

    /** @brief Get topic name by ID */
    inline std::optional<std::string> getTopicName(TopicId id, const BML_ImcBusInterface *bus = nullptr) {
        if (!BML_IMC_BUS_HAS_MEMBER(bus, GetTopicName)) return std::nullopt;
        char buffer[256] = {};
        size_t length = 0;
        const BML_Result result = bus->GetTopicName(id, buffer, sizeof(buffer), &length);
        if (result == BML_RESULT_OK) return std::string(buffer, length);
        if (result != BML_RESULT_BUFFER_TOO_SMALL || length == 0) return std::nullopt;

        std::vector<char> dynamicBuffer(length + 1, '\0');
        if (bus->GetTopicName(id, dynamicBuffer.data(), dynamicBuffer.size(), &length) != BML_RESULT_OK) {
            return std::nullopt;
        }
        return std::string(dynamicBuffer.data(), length);
    }

    /** @brief Format statistics as a debug string */
    inline std::string formatStats(const BML_ImcBusInterface *bus = nullptr) {
        auto s = getStats(bus);
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

    // ========================================================================
    // Convenience RPC free functions
    // ========================================================================

    /** @brief Call an RPC by name */
    inline RpcFuture callRpc(std::string_view name, const void *data = nullptr, size_t size = 0,
                             const BML_RpcInterface *rpc = nullptr) {
        return RpcClient(name, rpc).Call(data, size);
    }

    /** @brief Call a typed RPC by name */
    template <typename T>
    inline RpcFuture callRpc(std::string_view name, const T &req,
                             const BML_RpcInterface *rpc = nullptr) {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
        return RpcClient(name, rpc).Call(req);
    }

    /** @brief Synchronous typed RPC call */
    template <typename Resp, typename Req>
    inline std::optional<Resp> callRpcSync(std::string_view name, const Req &req,
                                            uint32_t timeoutMs = 5000,
                                            const BML_RpcInterface *rpc = nullptr) {
        return RpcClient(name, rpc).template CallSync<Resp, Req>(req, timeoutMs);
    }

    /** @brief Get info about a registered RPC endpoint */
    inline std::optional<BML_RpcInfo> getRpcInfo(RpcId id, const BML_RpcInterface *rpc = nullptr) {
        if (!BML_RPC_HAS_MEMBER(rpc, GetRpcInfo)) return std::nullopt;
        BML_RpcInfo info = BML_RPC_INFO_INIT;
        if (rpc->GetRpcInfo(id, &info) == BML_RESULT_OK) return info;
        return std::nullopt;
    }

    /** @brief Get RPC name by ID */
    inline std::optional<std::string> getRpcName(RpcId id, const BML_RpcInterface *rpc = nullptr) {
        if (!BML_RPC_HAS_MEMBER(rpc, GetRpcName)) return std::nullopt;
        char buffer[256] = {};
        size_t length = 0;
        const BML_Result result = rpc->GetRpcName(id, buffer, sizeof(buffer), &length);
        if (result == BML_RESULT_OK) return std::string(buffer, length);
        if (result != BML_RESULT_BUFFER_TOO_SMALL || length == 0) return std::nullopt;

        std::vector<char> dynamicBuffer(length + 1, '\0');
        if (rpc->GetRpcName(id, dynamicBuffer.data(), dynamicBuffer.size(), &length) != BML_RESULT_OK) {
            return std::nullopt;
        }
        return std::string(dynamicBuffer.data(), length);
    }

    // ========================================================================
    // Convenience publish (replaces Bus::Publish)
    // ========================================================================

    /** @brief Quick publish to a topic by name */
    inline bool publish(std::string_view topicName, const void *data = nullptr, size_t size = 0,
                        const BML_ImcBusInterface *bus = nullptr) {
        return Topic(topicName, bus).Publish(data, size);
    }

    /** @brief Quick publish typed data to a topic by name */
    template <typename T>
    inline bool publish(std::string_view topicName, const T &data, const BML_ImcBusInterface *bus = nullptr) {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
        return Topic(topicName, bus).Publish(data);
    }

} // namespace imc
} // namespace bml

#endif /* BML_IMC_HPP */
