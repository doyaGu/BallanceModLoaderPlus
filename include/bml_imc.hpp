/**
 * @file bml_imc.hpp
 * @brief BML C++ Inter-Mod Communication API
 *
 * This is the main header for the C++ IMC API. It includes all IMC components.
 *
 * @section imc_cpp_overview Overview
 *
 * The BML IMC C++ API provides a modern, type-safe, RAII-friendly interface
 * to the Inter-Module Communication system. It is organized into several
 * headers for modularity:
 *
 * - `bml_imc_fwd.hpp`          - Forward declarations and type aliases
 * - `bml_imc_message.hpp`      - Message types, builders, and zero-copy buffers
 * - `bml_imc_topic.hpp`        - Topic management and caching
 * - `bml_imc_subscription.hpp` - RAII subscription handles
 * - `bml_imc_publisher.hpp`    - High-level publishing abstractions
 * - `bml_imc_rpc.hpp`          - RPC client/server support
 * - `bml_imc_bus.hpp`          - Unified facade and diagnostics
 *
 * For most use cases, just include this header (`bml_imc.hpp`) to get
 * everything.
 *
 * @section imc_cpp_quickstart Quick Start
 *
 * @code
 * #include <bml_imc.hpp>
 *
 * using namespace bml::imc;
 *
 * // Publishing
 * Bus::publish("MyMod/Events/Update", &myData, sizeof(myData));
 *
 * // Typed publisher
 * Publisher<MyEvent> eventPub("MyMod/Events/Custom");
 * eventPub.publish(MyEvent{...});
 *
 * // Subscribing
 * auto sub = Bus::subscribe("OtherMod/Events", [](const Message& msg) {
 *     auto* data = msg.as<SomeData>();
 *     if (data) {
 *         // Handle data...
 *     }
 * });
 *
 * // Typed subscription
 * auto typedSub = Bus::subscribeTyped<MyEvent>("MyMod/Events/Custom",
 *     [](const MyEvent& event) {
 *         // Handle event...
 *     });
 *
 * // RPC
 * RpcClient client("OtherMod/GetValue");
 * auto result = client.callSync<int, int>(request, 1000);
 *
 * // Message pump (call each frame)
 * Bus::pump();
 * @endcode
 *
 * @section imc_cpp_patterns Common Patterns
 *
 * @subsection imc_cpp_event_driven Event-Driven Components
 * @code
 * class MyComponent {
 *     SubscriptionManager m_subs;
 *     Publisher<MyEvent> m_eventPub{"MyMod/Events"};
 *
 * public:
 *     void Initialize() {
 *         m_subs.add<TickEvent>("Core/Tick", [this](const TickEvent& e) {
 *             OnTick(e.deltaTime);
 *         });
 *         m_subs.add("Core/Render", [this](const Message& msg) {
 *             OnRender();
 *         });
 *     }
 *
 *     void FireEvent(const MyEvent& event) {
 *         m_eventPub.publish(event);
 *     }
 * };
 * @endcode
 *
 * @subsection imc_cpp_priority Priority Messages
 * @code
 * // Normal priority (default)
 * Bus::publish("Events/Normal", data);
 *
 * // High priority message
 * auto msg = MessageBuilder()
 *     .typed(data)
 *     .high()
 *     .build();
 * topic.publishEx(msg);
 *
 * // Or use publisher
 * publisher.publishHigh(data);
 * publisher.publishUrgent(criticalData);
 * @endcode
 *
 * @subsection imc_cpp_rpc_patterns RPC Patterns
 * @code
 * // Server side
 * RpcServer server("MyMod/GetHealth", [](const Message& req) {
 *     int playerId = *req.as<int>();
 *     int health = GetPlayerHealth(playerId);
 *
 *     std::vector<uint8_t> response(sizeof(health));
 *     std::memcpy(response.data(), &health, sizeof(health));
 *     return response;
 * });
 *
 * // Client side (async)
 * RpcClient client("MyMod/GetHealth");
 * auto future = client.call(playerId);
 * future.onComplete([](const Message& response) {
 *     int health = *response.as<int>();
 *     // Use health...
 * });
 *
 * // Client side (sync)
 * auto health = client.callSync<int, int>(playerId, 1000);
 * if (health) {
 *     // Use *health...
 * }
 * @endcode
 *
 * @section imc_cpp_thread_safety Thread Safety
 *
 * All IMC operations are thread-safe. The C++ wrappers add no additional
 * synchronization beyond what the underlying C API provides.
 *
 * - Publishing is always thread-safe
 * - Subscriptions can be created/destroyed from any thread
 * - Callbacks may be invoked from any thread (typically the pump thread)
 * - Topic/RPC ID resolution is thread-safe with internal caching
 */

#ifndef BML_IMC_HPP
#define BML_IMC_HPP

// Include all IMC components
#include "bml_imc_fwd.hpp"
#include "bml_imc_message.hpp"
#include "bml_imc_topic.hpp"
#include "bml_imc_subscription.hpp"
#include "bml_imc_publisher.hpp"
#include "bml_imc_rpc.hpp"
#include "bml_imc_bus.hpp"

#endif /* BML_IMC_HPP */
