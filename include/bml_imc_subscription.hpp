/**
 * @file bml_imc_subscription.hpp
 * @brief RAII subscription management for BML IMC
 *
 * Provides safe, flexible subscription handling with automatic cleanup.
 */

#ifndef BML_IMC_SUBSCRIPTION_HPP
#define BML_IMC_SUBSCRIPTION_HPP

#include "bml_imc_fwd.hpp"
#include "bml_imc_message.hpp"
#include "bml_imc_topic.hpp"
#include "bml_errors.h"

#include <memory>
#include <optional>

namespace bml {
namespace imc {

    // ========================================================================
    // Subscription Options Builder
    // ========================================================================

    /**
     * @brief Builder for subscription options
     *
     * Fluent interface for configuring subscription behavior.
     *
     * @code
     * auto opts = SubscribeOptions()
     *     .QueueCapacity(512)
     *     .Backpressure(backpressure::DropOldest)
     *     .MinPriority(priority::Normal)
     *     .Filter([](const Message& msg) { return msg.Size() > 0; });
     * @endcode
     */
    class SubscribeOptions {
    public:
        SubscribeOptions() {
            m_Opts = BML_SUBSCRIBE_OPTIONS_INIT;
        }

        /**
         * @brief Set queue capacity
         * @param capacity Queue size (0 = default 256)
         */
        SubscribeOptions &QueueCapacity(uint32_t capacity) & {
            m_Opts.queue_capacity = capacity;
            return *this;
        }

        SubscribeOptions &&QueueCapacity(uint32_t capacity) && {
            return std::move(QueueCapacity(capacity));
        }

        /**
         * @brief Set backpressure policy
         */
        SubscribeOptions &Backpressure(BackpressurePolicy policy) & {
            m_Opts.backpressure = policy;
            return *this;
        }

        SubscribeOptions &&Backpressure(BackpressurePolicy policy) && {
            return std::move(Backpressure(policy));
        }

        /**
         * @brief Set minimum priority filter
         */
        SubscribeOptions &MinPriority(Priority p) & {
            m_Opts.min_priority = p;
            return *this;
        }

        SubscribeOptions &&MinPriority(Priority p) && {
            return std::move(MinPriority(p));
        }

        /**
         * @brief Set message filter
         * @note Filter is stored and must outlive the subscription
         */
        SubscribeOptions &Filter(BML_ImcFilter fn, void *userData = nullptr) & {
            m_Opts.filter = fn;
            m_Opts.filter_user_data = userData;
            return *this;
        }

        SubscribeOptions &&Filter(BML_ImcFilter fn, void *userData = nullptr) && {
            return std::move(Filter(fn, userData));
        }

        const BML_SubscribeOptions &Native() const noexcept { return m_Opts; }
        const BML_SubscribeOptions *NativePtr() const noexcept { return &m_Opts; }

    private:
        BML_SubscribeOptions m_Opts;
    };

    // ========================================================================
    // Subscription Handle
    // ========================================================================

    namespace detail {

        /**
         * @internal Callback wrapper for type erasure
         */
        struct SubscriptionContext {
            MessageCallback callback;
            MessageFilterPredicate filter;

            static void Invoke(BML_Context, BML_TopicId, const BML_ImcMessage *msg, void *userData) {
                auto *ctx = static_cast<SubscriptionContext *>(userData);
                if (ctx && ctx->callback && msg) {
                    Message wrapped(msg);
                    if (!ctx->filter || ctx->filter(wrapped)) {
                        ctx->callback(wrapped);
                    }
                }
            }
        };

    } // namespace detail

    /**
     * @brief RAII subscription handle
     *
     * Automatically unsubscribes when destroyed. Move-only.
     *
     * @code
     * Subscription sub = Subscription::create("MyTopic", [](const Message& msg) {
     *     auto data = msg.As<MyData>();
     *     // Handle message...
     * });
     *
     * // Check if active
     * if (sub.IsActive()) {
     *     // Still receiving messages
     * }
     *
     * // Manual unsubscribe (also happens on destruction)
     * sub.Unsubscribe();
     * @endcode
     */
    class Subscription {
    public:
        /**
         * @brief Default constructor (invalid subscription)
         */
        Subscription() : m_Handle(nullptr), m_TopicId(InvalidTopicId) {}

        /**
         * @brief Move constructor
         */
        Subscription(Subscription &&other) noexcept
            : m_Handle(other.m_Handle),
              m_TopicId(other.m_TopicId),
              m_TopicName(std::move(other.m_TopicName)),
              m_Context(std::move(other.m_Context)) {
            other.m_Handle = nullptr;
            other.m_TopicId = InvalidTopicId;
        }

        /**
         * @brief Move assignment
         */
        Subscription &operator=(Subscription &&other) noexcept {
            if (this != &other) {
                Unsubscribe();
                m_Handle = other.m_Handle;
                m_TopicId = other.m_TopicId;
                m_TopicName = std::move(other.m_TopicName);
                m_Context = std::move(other.m_Context);
                other.m_Handle = nullptr;
                other.m_TopicId = InvalidTopicId;
            }
            return *this;
        }

        // Non-copyable
        Subscription(const Subscription &) = delete;
        Subscription &operator=(const Subscription &) = delete;

        /**
         * @brief Destructor - automatically unsubscribes
         */
        ~Subscription() {
            Unsubscribe();
        }

        // ====================================================================
        // Factory Methods
        // ====================================================================

        /**
         * @brief Create subscription with Message callback
         */
        static std::optional<Subscription> create(
            std::string_view topicName,
            MessageCallback callback,
            const SubscribeOptions *options = nullptr
        ) {
            if (!bmlImcSubscribe || !bmlImcGetTopicId) return std::nullopt;

            TopicId topicId;
            if (bmlImcGetTopicId(topicName.data(), &topicId) != BML_RESULT_OK) {
                return std::nullopt;
            }

            return createWithId(topicId, std::string(topicName), std::move(callback), options);
        }

        /**
         * @brief Create subscription from Topic
         */
        static std::optional<Subscription> create(
            const Topic &topic,
            MessageCallback callback,
            const SubscribeOptions *options = nullptr
        ) {
            if (!topic.Valid()) return std::nullopt;
            return createWithId(topic.Id(), topic.Name(), std::move(callback), options);
        }

        /**
         * @brief Create subscription with simple callback (data + size)
         */
        static std::optional<Subscription> createSimple(
            std::string_view topicName,
            SimpleCallback callback,
            const SubscribeOptions *options = nullptr
        ) {
            return create(topicName, [cb = std::move(callback)](const Message &msg) {
                cb(msg.Data(), msg.Size());
            }, options);
        }

        /**
         * @brief Create subscription with typed callback
         */
        template <typename T>
        static std::optional<Subscription> createTyped(
            std::string_view topicName,
            TypedCallback<T> callback,
            const SubscribeOptions *options = nullptr
        ) {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            return create(topicName, [cb = std::move(callback)](const Message &msg) {
                if (auto *data = msg.As<T>()) {
                    cb(*data);
                }
            }, options);
        }

        // ====================================================================
        // Operations
        // ====================================================================

        /**
         * @brief Unsubscribe and release the subscription
         */
        void Unsubscribe() {
            if (m_Handle && bmlImcUnsubscribe) {
                bmlImcUnsubscribe(m_Handle);
                m_Handle = nullptr;
                m_TopicId = InvalidTopicId;
            }
        }

        /**
         * @brief Check if subscription is still active
         */
        bool IsActive() const {
            if (!m_Handle || !bmlImcSubscriptionIsActive) return false;
            BML_Bool active = BML_FALSE;
            bmlImcSubscriptionIsActive(m_Handle, &active);
            return active == BML_TRUE;
        }

        /**
         * @brief Check if subscription handle is valid
         */
        bool Valid() const noexcept { return m_Handle != nullptr; }

        explicit operator bool() const noexcept { return Valid(); }

        // ====================================================================
        // Properties
        // ====================================================================

        TopicId topicId() const noexcept { return m_TopicId; }
        const std::string &topicName() const noexcept { return m_TopicName; }
        BML_Subscription Native() const noexcept { return m_Handle; }

        // ====================================================================
        // Statistics
        // ====================================================================

        /**
         * @brief Get subscription statistics
         */
        std::optional<BML_SubscriptionStats> Stats() const {
            if (!m_Handle || !bmlImcGetSubscriptionStats) return std::nullopt;
            BML_SubscriptionStats s = BML_SUBSCRIPTION_STATS_INIT;
            if (bmlImcGetSubscriptionStats(m_Handle, &s) == BML_RESULT_OK) {
                return s;
            }
            return std::nullopt;
        }

        /**
         * @brief Get current queue size
         */
        size_t QueueSize() const {
            auto s = Stats();
            return s ? s->queue_size : 0;
        }

        /**
         * @brief Get total received message count
         */
        uint64_t ReceivedCount() const {
            auto s = Stats();
            return s ? s->messages_received : 0;
        }

        /**
         * @brief Get dropped message count
         */
        uint64_t DroppedCount() const {
            auto s = Stats();
            return s ? s->messages_dropped : 0;
        }

    private:
        BML_Subscription m_Handle;
        TopicId m_TopicId;
        std::string m_TopicName;
        std::unique_ptr<detail::SubscriptionContext> m_Context;

        static std::optional<Subscription> createWithId(
            TopicId topicId,
            std::string topicName,
            MessageCallback callback,
            const SubscribeOptions *options
        ) {
            Subscription sub;
            sub.m_TopicId = topicId;
            sub.m_TopicName = std::move(topicName);
            sub.m_Context = std::make_unique<detail::SubscriptionContext>();
            sub.m_Context->callback = std::move(callback);

            BML_Result result;
            if (options && bmlImcSubscribeEx) {
                result = bmlImcSubscribeEx(
                    topicId,
                    detail::SubscriptionContext::Invoke,
                    sub.m_Context.get(),
                    options->NativePtr(),
                    &sub.m_Handle
                );
            } else if (bmlImcSubscribe) {
                result = bmlImcSubscribe(
                    topicId,
                    detail::SubscriptionContext::Invoke,
                    sub.m_Context.get(),
                    &sub.m_Handle
                );
            } else {
                return std::nullopt;
            }

            if (result != BML_RESULT_OK) {
                return std::nullopt;
            }

            return sub;
        }
    };

    // ========================================================================
    // Subscription Guard (for temporary subscriptions)
    // ========================================================================

    /**
     * @brief Scoped subscription that automatically cleans up
     *
     * Use when you need a subscription for a limited scope.
     *
     * @code
     * {
     *     SubscriptionGuard guard("Events/Temporary", [](const Message& msg) {
     *         // Handle during this scope only
     *     });
     *
     *     DoSomeWork();  // Subscription active here
     * }  // Automatically unsubscribed
     * @endcode
     */
    class SubscriptionGuard {
    public:
        SubscriptionGuard(std::string_view topicName, MessageCallback callback)
            : m_Sub(Subscription::create(topicName, std::move(callback))) {}

        SubscriptionGuard(const Topic &topic, MessageCallback callback)
            : m_Sub(Subscription::create(topic, std::move(callback))) {}

        bool Valid() const noexcept { return m_Sub && m_Sub->Valid(); }
        explicit operator bool() const noexcept { return Valid(); }

        Subscription *operator->() { return m_Sub ? &*m_Sub : nullptr; }
        const Subscription *operator->() const { return m_Sub ? &*m_Sub : nullptr; }

    private:
        std::optional<Subscription> m_Sub;
    };

    // ========================================================================
    // Multi-Subscription Manager
    // ========================================================================

    /**
     * @brief Manages multiple subscriptions with automatic cleanup
     *
     * Useful when a mod needs to subscribe to many topics.
     *
     * @code
     * SubscriptionManager subs;
     *
     * subs.Add("Events/Tick", [this](const Message& msg) { OnTick(msg); });
     * subs.Add("Events/Render", [this](const Message& msg) { OnRender(msg); });
     * subs.add<PhysicsData>("Events/Physics", [this](const PhysicsData& d) { OnPhysics(d); });
     *
     * // All unsubscribed when subs goes out of scope
     * @endcode
     */
    class SubscriptionManager {
    public:
        SubscriptionManager() = default;
        ~SubscriptionManager() = default;

        // Move-only
        SubscriptionManager(SubscriptionManager &&) = default;
        SubscriptionManager &operator=(SubscriptionManager &&) = default;
        SubscriptionManager(const SubscriptionManager &) = delete;
        SubscriptionManager &operator=(const SubscriptionManager &) = delete;

        /**
         * @brief Add a subscription
         * @return true if successful
         */
        bool Add(std::string_view topicName, MessageCallback callback) {
            if (auto sub = Subscription::create(topicName, std::move(callback))) {
                m_Subscriptions.push_back(std::move(*sub));
                return true;
            }
            return false;
        }

        /**
         * @brief Add a subscription from Topic
         */
        bool Add(const Topic &topic, MessageCallback callback) {
            if (auto sub = Subscription::create(topic, std::move(callback))) {
                m_Subscriptions.push_back(std::move(*sub));
                return true;
            }
            return false;
        }

        /**
         * @brief Add a typed subscription
         */
        template <typename T>
        bool Add(std::string_view topicName, TypedCallback<T> callback) {
            if (auto sub = Subscription::createTyped<T>(topicName, std::move(callback))) {
                m_Subscriptions.push_back(std::move(*sub));
                return true;
            }
            return false;
        }

        /**
         * @brief Add a simple (data + size) subscription
         */
        bool AddSimple(std::string_view topicName, SimpleCallback callback) {
            if (auto sub = Subscription::createSimple(topicName, std::move(callback))) {
                m_Subscriptions.push_back(std::move(*sub));
                return true;
            }
            return false;
        }

        /**
         * @brief Unsubscribe all
         */
        void Clear() {
            m_Subscriptions.clear();
        }

        /**
         * @brief Get number of active subscriptions
         */
        size_t Count() const noexcept { return m_Subscriptions.size(); }

        /**
         * @brief Check if empty
         */
        bool Empty() const noexcept { return m_Subscriptions.empty(); }

        /**
         * @brief Access subscription by index
         */
        Subscription &operator[](size_t index) { return m_Subscriptions[index]; }
        const Subscription &operator[](size_t index) const { return m_Subscriptions[index]; }

        // Range-based for support
        auto begin() { return m_Subscriptions.begin(); }
        auto end() { return m_Subscriptions.end(); }
        auto begin() const { return m_Subscriptions.begin(); }
        auto end() const { return m_Subscriptions.end(); }

    private:
        std::vector<Subscription> m_Subscriptions;
    };

} // namespace imc
} // namespace bml

#endif /* BML_IMC_SUBSCRIPTION_HPP */

