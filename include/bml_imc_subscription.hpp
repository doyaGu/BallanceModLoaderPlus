/**
 * @file bml_imc_subscription.hpp
 * @brief RAII subscription management for BML IMC (regular + intercept)
 */

#ifndef BML_IMC_SUBSCRIPTION_HPP
#define BML_IMC_SUBSCRIPTION_HPP

#include "bml_imc_fwd.hpp"
#include "bml_imc_message.hpp"
#include "bml_imc_topic.hpp"

#include <memory>
#include <optional>
#include <vector>

namespace bml {
namespace imc {

    // ========================================================================
    // Subscription Options Builder
    // ========================================================================

    class SubscribeOptions {
    public:
        SubscribeOptions() { m_Opts = BML_SUBSCRIBE_OPTIONS_INIT; }

        SubscribeOptions &QueueCapacity(uint32_t capacity) { m_Opts.queue_capacity = capacity; return *this; }
        SubscribeOptions &Backpressure(BackpressurePolicy policy) { m_Opts.backpressure = policy; return *this; }
        SubscribeOptions &MinPriority(Priority p) { m_Opts.min_priority = p; return *this; }
        SubscribeOptions &ExecutionOrder(int32_t order) { m_Opts.execution_order = order; return *this; }
        SubscribeOptions &Flags(uint32_t flags) { m_Opts.flags = flags; return *this; }
        SubscribeOptions &Filter(BML_ImcFilter fn, void *userData = nullptr) {
            m_Opts.filter = fn;
            m_Opts.filter_user_data = userData;
            return *this;
        }

        const BML_SubscribeOptions &Native() const noexcept { return m_Opts; }
        const BML_SubscribeOptions *NativePtr() const noexcept { return &m_Opts; }

    private:
        BML_SubscribeOptions m_Opts;
    };

    // ========================================================================
    // Internal callback wrappers
    // ========================================================================

    namespace detail {

        struct SubscriptionContext {
            MessageCallback callback;
            MessageFilterPredicate filter;

            static void Invoke(BML_Context, BML_TopicId, const BML_ImcMessage *msg, void *userData) {
                auto *ctx = static_cast<SubscriptionContext *>(userData);
                if (ctx && ctx->callback && msg) {
                    Message wrapped(msg);
                    if (!ctx->filter || ctx->filter(wrapped))
                        ctx->callback(wrapped);
                }
            }
        };

        struct InterceptContext {
            InterceptCallback callback;

            static BML_EventResult Invoke(BML_Context, BML_TopicId, BML_ImcMessage *msg, void *userData) {
                auto *ctx = static_cast<InterceptContext *>(userData);
                if (ctx && ctx->callback && msg) {
                    MutableMessage wrapped(msg);
                    return ctx->callback(wrapped);
                }
                return BML_EVENT_CONTINUE;
            }
        };

    } // namespace detail

    // ========================================================================
    // Subscription Handle (regular pub/sub)
    // ========================================================================

    class Subscription {
    public:
        Subscription() : m_Handle(nullptr), m_TopicId(InvalidTopicId) {}

        Subscription(Subscription &&other) noexcept
            : m_Handle(other.m_Handle), m_TopicId(other.m_TopicId),
              m_TopicName(std::move(other.m_TopicName)),
              m_Context(std::move(other.m_Context)), m_Bus(other.m_Bus) {
            other.m_Handle = nullptr;
            other.m_TopicId = InvalidTopicId;
            other.m_Bus = nullptr;
        }

        Subscription &operator=(Subscription &&other) noexcept {
            if (this != &other) {
                Unsubscribe();
                m_Handle = other.m_Handle;
                m_TopicId = other.m_TopicId;
                m_TopicName = std::move(other.m_TopicName);
                m_Context = std::move(other.m_Context);
                m_Bus = other.m_Bus;
                other.m_Handle = nullptr;
                other.m_TopicId = InvalidTopicId;
                other.m_Bus = nullptr;
            }
            return *this;
        }

        Subscription(const Subscription &) = delete;
        Subscription &operator=(const Subscription &) = delete;
        ~Subscription() { Unsubscribe(); }

        // Factory: Message callback
        static std::optional<Subscription> create(
            std::string_view topicName, MessageCallback callback,
            const SubscribeOptions *options = nullptr,
            const BML_ImcBusInterface *bus = nullptr
        ) {
            if (!bus || !bus->GetTopicId) return std::nullopt;
            std::string resolvedName(topicName);
            TopicId topicId;
            if (bus->GetTopicId(resolvedName.c_str(), &topicId) != BML_RESULT_OK) return std::nullopt;
            return createWithId(topicId, std::move(resolvedName), std::move(callback), options, bus);
        }

        static std::optional<Subscription> create(
            const Topic &topic, MessageCallback callback,
            const SubscribeOptions *options = nullptr,
            const BML_ImcBusInterface *bus = nullptr
        ) {
            if (!topic.Valid()) return std::nullopt;
            return createWithId(topic.Id(), topic.Name(), std::move(callback), options,
                                bus ? bus : topic.Bus());
        }

        // Factory: Simple callback
        static std::optional<Subscription> createSimple(
            std::string_view topicName, SimpleCallback callback,
            const SubscribeOptions *options = nullptr,
            const BML_ImcBusInterface *bus = nullptr
        ) {
            return create(topicName, [cb = std::move(callback)](const Message &msg) {
                cb(msg.Data(), msg.Size());
            }, options, bus);
        }

        // Factory: Typed callback
        template <typename T>
        static std::optional<Subscription> createTyped(
            std::string_view topicName, TypedCallback<T> callback,
            const SubscribeOptions *options = nullptr,
            const BML_ImcBusInterface *bus = nullptr
        ) {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            return create(topicName, [cb = std::move(callback)](const Message &msg) {
                if (auto *data = msg.As<T>()) cb(*data);
            }, options, bus);
        }

        // Factory: Intercept callback
        static std::optional<Subscription> createIntercept(
            std::string_view topicName, InterceptCallback callback,
            const SubscribeOptions *options = nullptr,
            const BML_ImcBusInterface *bus = nullptr
        ) {
            if (!bus || !bus->GetTopicId) return std::nullopt;
            std::string resolvedName(topicName);
            TopicId topicId;
            if (bus->GetTopicId(resolvedName.c_str(), &topicId) != BML_RESULT_OK) return std::nullopt;
            return createInterceptWithId(topicId, std::move(resolvedName), std::move(callback), options, bus);
        }

        static std::optional<Subscription> createIntercept(
            const Topic &topic, InterceptCallback callback,
            const SubscribeOptions *options = nullptr,
            const BML_ImcBusInterface *bus = nullptr
        ) {
            if (!topic.Valid()) return std::nullopt;
            return createInterceptWithId(topic.Id(), topic.Name(), std::move(callback), options,
                                         bus ? bus : topic.Bus());
        }

        // Factory: Typed intercept
        template <typename T>
        static std::optional<Subscription> createTypedIntercept(
            std::string_view topicName, TypedInterceptCallback<T> callback,
            const SubscribeOptions *options = nullptr,
            const BML_ImcBusInterface *bus = nullptr
        ) {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            return createIntercept(topicName, [cb = std::move(callback)](MutableMessage &msg) -> EventResult {
                if (const auto *data = msg.As<T>()) return cb(*data);
                return event_result::Continue;
            }, options, bus);
        }

        // Operations
        void Unsubscribe() {
            if (m_Handle && m_Bus && m_Bus->Unsubscribe) {
                m_Bus->Unsubscribe(m_Handle);
                m_Handle = nullptr;
                m_TopicId = InvalidTopicId;
            }
        }

        bool IsActive() const {
            if (!m_Handle || !m_Bus || !m_Bus->SubscriptionIsActive) return false;
            BML_Bool active = BML_FALSE;
            m_Bus->SubscriptionIsActive(m_Handle, &active);
            return active == BML_TRUE;
        }

        bool Valid() const noexcept { return m_Handle != nullptr; }
        explicit operator bool() const noexcept { return Valid(); }

        TopicId topicId() const noexcept { return m_TopicId; }
        const std::string &topicName() const noexcept { return m_TopicName; }
        BML_Subscription Native() const noexcept { return m_Handle; }

        std::optional<BML_SubscriptionStats> Stats() const {
            if (!m_Handle || !m_Bus || !m_Bus->GetSubscriptionStats) return std::nullopt;
            BML_SubscriptionStats s = BML_SUBSCRIPTION_STATS_INIT;
            if (m_Bus->GetSubscriptionStats(m_Handle, &s) == BML_RESULT_OK) return s;
            return std::nullopt;
        }

        size_t QueueSize() const { auto s = Stats(); return s ? s->queue_size : 0; }
        uint64_t ReceivedCount() const { auto s = Stats(); return s ? s->messages_received : 0; }
        uint64_t DroppedCount() const { auto s = Stats(); return s ? s->messages_dropped : 0; }

    private:
        BML_Subscription m_Handle;
        TopicId m_TopicId;
        std::string m_TopicName;
        std::unique_ptr<detail::SubscriptionContext> m_Context;
        std::unique_ptr<detail::InterceptContext> m_InterceptContext;
        const BML_ImcBusInterface *m_Bus = nullptr;

        static std::optional<Subscription> createWithId(
            TopicId topicId, std::string topicName, MessageCallback callback,
            const SubscribeOptions *options, const BML_ImcBusInterface *bus
        ) {
            if (!bus) return std::nullopt;
            Subscription sub;
            sub.m_TopicId = topicId;
            sub.m_TopicName = std::move(topicName);
            sub.m_Context = std::make_unique<detail::SubscriptionContext>();
            sub.m_Context->callback = std::move(callback);
            sub.m_Bus = bus;

            BML_Result result;
            if (options && bus->SubscribeEx) {
                result = bus->SubscribeEx(topicId, detail::SubscriptionContext::Invoke,
                                          sub.m_Context.get(), options->NativePtr(), &sub.m_Handle);
            } else if (bus->Subscribe) {
                result = bus->Subscribe(topicId, detail::SubscriptionContext::Invoke,
                                        sub.m_Context.get(), &sub.m_Handle);
            } else if (bus->SubscribeEx) {
                SubscribeOptions defaultOptions;
                result = bus->SubscribeEx(topicId, detail::SubscriptionContext::Invoke,
                                          sub.m_Context.get(), defaultOptions.NativePtr(), &sub.m_Handle);
            } else {
                return std::nullopt;
            }
            return (result == BML_RESULT_OK) ? std::optional(std::move(sub)) : std::nullopt;
        }

        static std::optional<Subscription> createInterceptWithId(
            TopicId topicId, std::string topicName, InterceptCallback callback,
            const SubscribeOptions *options, const BML_ImcBusInterface *bus
        ) {
            if (!bus) return std::nullopt;
            Subscription sub;
            sub.m_TopicId = topicId;
            sub.m_TopicName = std::move(topicName);
            sub.m_InterceptContext = std::make_unique<detail::InterceptContext>();
            sub.m_InterceptContext->callback = std::move(callback);
            sub.m_Bus = bus;

            BML_Result result;
            if (options && bus->SubscribeInterceptEx) {
                result = bus->SubscribeInterceptEx(topicId, detail::InterceptContext::Invoke,
                                                   sub.m_InterceptContext.get(),
                                                   options->NativePtr(), &sub.m_Handle);
            } else if (bus->SubscribeIntercept) {
                result = bus->SubscribeIntercept(topicId, detail::InterceptContext::Invoke,
                                                 sub.m_InterceptContext.get(), &sub.m_Handle);
            } else if (bus->SubscribeInterceptEx) {
                SubscribeOptions defaultOptions;
                result = bus->SubscribeInterceptEx(topicId, detail::InterceptContext::Invoke,
                                                   sub.m_InterceptContext.get(),
                                                   defaultOptions.NativePtr(), &sub.m_Handle);
            } else {
                return std::nullopt;
            }
            return (result == BML_RESULT_OK) ? std::optional(std::move(sub)) : std::nullopt;
        }
    };

    // ========================================================================
    // Multi-Subscription Manager
    // ========================================================================

    class SubscriptionManager {
    public:
        explicit SubscriptionManager(const BML_ImcBusInterface *bus = nullptr) : m_Bus(bus) {}
        ~SubscriptionManager() = default;
        SubscriptionManager(SubscriptionManager &&) = default;
        SubscriptionManager &operator=(SubscriptionManager &&) = default;
        SubscriptionManager(const SubscriptionManager &) = delete;
        SubscriptionManager &operator=(const SubscriptionManager &) = delete;

        bool Add(std::string_view topicName, MessageCallback callback) {
            if (auto sub = Subscription::create(topicName, std::move(callback), nullptr, m_Bus)) {
                m_Subs.push_back(std::move(*sub));
                return true;
            }
            return false;
        }

        bool Add(const Topic &topic, MessageCallback callback) {
            if (auto sub = Subscription::create(topic, std::move(callback), nullptr, m_Bus)) {
                m_Subs.push_back(std::move(*sub));
                return true;
            }
            return false;
        }

        template <typename T>
        bool Add(std::string_view topicName, TypedCallback<T> callback) {
            if (auto sub = Subscription::createTyped<T>(topicName, std::move(callback), nullptr, m_Bus)) {
                m_Subs.push_back(std::move(*sub));
                return true;
            }
            return false;
        }

        bool AddSimple(std::string_view topicName, SimpleCallback callback) {
            if (auto sub = Subscription::createSimple(topicName, std::move(callback), nullptr, m_Bus)) {
                m_Subs.push_back(std::move(*sub));
                return true;
            }
            return false;
        }

        bool AddIntercept(std::string_view topicName, InterceptCallback callback,
                          const SubscribeOptions *options = nullptr) {
            if (auto sub = Subscription::createIntercept(topicName, std::move(callback), options, m_Bus)) {
                m_Subs.push_back(std::move(*sub));
                return true;
            }
            return false;
        }

        template <typename T>
        bool AddIntercept(std::string_view topicName, TypedInterceptCallback<T> callback,
                          const SubscribeOptions *options = nullptr) {
            if (auto sub = Subscription::createTypedIntercept<T>(topicName, std::move(callback), options, m_Bus)) {
                m_Subs.push_back(std::move(*sub));
                return true;
            }
            return false;
        }

        void Bind(const BML_ImcBusInterface *bus) { m_Bus = bus; }
        void Clear() { m_Subs.clear(); }
        size_t Count() const noexcept { return m_Subs.size(); }
        bool Empty() const noexcept { return m_Subs.empty(); }

        Subscription &operator[](size_t index) { return m_Subs[index]; }
        const Subscription &operator[](size_t index) const { return m_Subs[index]; }

        auto begin() { return m_Subs.begin(); }
        auto end() { return m_Subs.end(); }
        auto begin() const { return m_Subs.begin(); }
        auto end() const { return m_Subs.end(); }

    private:
        const BML_ImcBusInterface *m_Bus = nullptr;
        std::vector<Subscription> m_Subs;
    };

} // namespace imc
} // namespace bml

#endif /* BML_IMC_SUBSCRIPTION_HPP */
