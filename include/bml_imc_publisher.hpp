/**
 * @file bml_imc_publisher.hpp
 * @brief High-level publisher abstraction for BML IMC
 *
 * Provides convenient, type-safe publishing with optional batching.
 */

#ifndef BML_IMC_PUBLISHER_HPP
#define BML_IMC_PUBLISHER_HPP

#include "bml_imc_fwd.hpp"
#include "bml_imc_topic.hpp"
#include "bml_imc_message.hpp"

#include <vector>
#include <initializer_list>

namespace bml {
namespace imc {

    // ========================================================================
    // Publisher
    // ========================================================================

    /**
     * @brief High-level publisher for a specific topic
     *
     * Provides a convenient interface for publishing messages to a topic.
     * Caches the topic ID for efficient repeated publishing.
     *
     * @code
     * Publisher<PhysicsEvent> physicsPublisher("Physics/Events");
     *
     * // Simple typed publish
     * physicsPublisher.Publish(PhysicsEvent{...});
     *
     * // With priority
     * physicsPublisher.Publish(event, priority::High);
     *
     * // With full options
     * physicsPublisher.PublishEx(
     *     MessageBuilder().typed(event).high().addFlags(flags::Reliable)
     * );
     * @endcode
     *
     * @tparam T Message payload type (must be trivially copyable)
     */
    template <typename T = void>
    class Publisher {
    public:
        static_assert(std::is_void_v<T> || std::is_trivially_copyable_v<T>,
                      "T must be trivially copyable or void");

        Publisher() = default;

        /**
         * @brief Construct publisher for a topic
         */
        explicit Publisher(std::string_view topicName)
            : m_Topic(topicName) {}

        explicit Publisher(const Topic &topic)
            : m_Topic(topic) {}

        // ====================================================================
        // Properties
        // ====================================================================

        bool Valid() const noexcept { return m_Topic.Valid(); }
        explicit operator bool() const noexcept { return Valid(); }

        const Topic &topic() const noexcept { return m_Topic; }
        TopicId topicId() const noexcept { return m_Topic.id(); }

        // ====================================================================
        // Publishing (Typed)
        // ====================================================================

        /**
         * @brief Publish typed data
         */
        template <typename U = T>
        std::enable_if_t<!std::is_void_v<U>, bool>
        Publish(const U &data) {
            return m_Topic.Publish(data);
        }

        /**
         * @brief Publish typed data with priority
         */
        template <typename U = T>
        std::enable_if_t<!std::is_void_v<U>, bool>
        Publish(const U &data, Priority prio) {
            BML_ImcMessage msg = BML_IMC_MSG(&data, sizeof(U));
            msg.priority = prio;
            return m_Topic.PublishEx(msg);
        }

        // ====================================================================
        // Publishing (Raw)
        // ====================================================================

        /**
         * @brief Publish raw data
         */
        bool PublishRaw(const void *data = nullptr, size_t size = 0) {
            return m_Topic.Publish(data, size);
        }

        /**
         * @brief Publish string
         */
        bool PublishString(std::string_view str) {
            return m_Topic.PublishString(str);
        }

        /**
         * @brief Publish with message builder
         */
        bool PublishEx(const MessageBuilder &builder) {
            return m_Topic.PublishEx(builder);
        }

        /**
         * @brief Publish with message struct
         */
        bool PublishEx(const BML_ImcMessage &msg) {
            return m_Topic.PublishEx(msg);
        }

        /**
         * @brief Publish zero-copy buffer
         */
        bool PublishBuffer(const ZeroCopyBuffer &buffer) {
            return m_Topic.PublishBuffer(buffer);
        }

        // ====================================================================
        // Convenience Methods
        // ====================================================================

        /**
         * @brief Publish with high priority
         */
        template <typename U = T>
        std::enable_if_t<!std::is_void_v<U>, bool>
        PublishHigh(const U &data) {
            return Publish(data, priority::High);
        }

        /**
         * @brief Publish with urgent priority
         */
        template <typename U = T>
        std::enable_if_t<!std::is_void_v<U>, bool>
        PublishUrgent(const U &data) {
            return Publish(data, priority::Urgent);
        }

        /**
         * @brief Notify (publish empty message)
         */
        bool Notify() {
            return PublishRaw();
        }

        /**
         * @brief Notify with priority
         */
        bool Notify(Priority prio) {
            BML_ImcMessage msg = BML_IMC_MESSAGE_INIT;
            msg.priority = prio;
            return m_Topic.PublishEx(msg);
        }

    private:
        Topic m_Topic;
    };

    // ========================================================================
    // Void Specialization
    // ========================================================================

    /**
     * @brief Publisher without typed payload
     *
     * Use for notifications or when payload type varies.
     */
    template <>
    class Publisher<void> {
    public:
        Publisher() = default;

        explicit Publisher(std::string_view topicName)
            : m_Topic(topicName) {}

        explicit Publisher(const Topic &topic)
            : m_Topic(topic) {}

        bool Valid() const noexcept { return m_Topic.Valid(); }
        explicit operator bool() const noexcept { return Valid(); }

        const Topic &GetTopic() const noexcept { return m_Topic; }
        TopicId GetTopicId() const noexcept { return m_Topic.Id(); }

        /**
         * @brief Publish any typed data
         */
        template <typename T>
        bool Publish(const T &data) {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            return m_Topic.Publish(data);
        }

        template <typename T>
        bool Publish(const T &data, Priority prio) {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            BML_ImcMessage msg = BML_IMC_MSG(&data, sizeof(T));
            msg.priority = prio;
            return m_Topic.PublishEx(msg);
        }

        bool PublishRaw(const void *data = nullptr, size_t size = 0) {
            return m_Topic.Publish(data, size);
        }

        bool PublishString(std::string_view str) {
            return m_Topic.PublishString(str);
        }

        bool PublishEx(const MessageBuilder &builder) {
            return m_Topic.PublishEx(builder);
        }

        bool PublishEx(const BML_ImcMessage &msg) {
            return m_Topic.PublishEx(msg);
        }

        bool PublishBuffer(const ZeroCopyBuffer &buffer) {
            return m_Topic.PublishBuffer(buffer);
        }

        bool Notify() {
            return PublishRaw();
        }

        bool Notify(Priority prio) {
            BML_ImcMessage msg = BML_IMC_MESSAGE_INIT;
            msg.priority = prio;
            return m_Topic.PublishEx(msg);
        }

    private:
        Topic m_Topic;
    };

    // ========================================================================
    // Multi-Publisher
    // ========================================================================

    /**
     * @brief Publish to multiple topics simultaneously
     *
     * @code
     * MultiPublisher multi({"Events/A", "Events/B", "Events/C"});
     *
     * // Publish to all topics
     * multi.Publish(&data, sizeof(data));
     *
     * // Or use the batch API
     * multi.publishMulti(&data, sizeof(data));
     * @endcode
     */
    class MultiPublisher {
    public:
        MultiPublisher() = default;

        explicit MultiPublisher(std::initializer_list<std::string_view> topicNames) {
            for (auto name : topicNames) {
                Add(name);
            }
        }

        explicit MultiPublisher(const std::vector<std::string_view> &topicNames) {
            for (auto name : topicNames) {
                Add(name);
            }
        }

        /**
         * @brief Add a topic
         */
        bool Add(std::string_view name) {
            Topic topic(name);
            if (topic.Valid()) {
                m_Topics.push_back(topic);
                return true;
            }
            return false;
        }

        bool Add(const Topic &topic) {
            if (topic.Valid()) {
                m_Topics.push_back(topic);
                return true;
            }
            return false;
        }

        /**
         * @brief Publish to all topics (one by one)
         * @return Number of successful publishes
         */
        size_t Publish(const void *data = nullptr, size_t size = 0) {
            size_t count = 0;
            for (const auto &topic : m_Topics) {
                if (topic.Publish(data, size)) {
                    ++count;
                }
            }
            return count;
        }

        /**
         * @brief Publish typed data to all topics
         */
        template <typename T>
        size_t Publish(const T &data) {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            return Publish(&data, sizeof(T));
        }

        /**
         * @brief Publish to all topics atomically (batch API)
         * @return Number of successful deliveries
         */
        size_t PublishMulti(const void *data, size_t size, const BML_ImcMessage *msg = nullptr) {
            if (m_Topics.empty() || !bmlImcPublishMulti) {
                return Publish(data, size);  // Fallback to one-by-one
            }

            std::vector<TopicId> ids;
            ids.reserve(m_Topics.size());
            for (const auto &topic : m_Topics) {
                ids.push_back(topic.Id());
            }

            size_t delivered = 0;
            if (bmlImcPublishMulti(ids.data(), ids.size(), data, size, msg, &delivered) == BML_RESULT_OK) {
                return delivered;
            }
            return 0;
        }

        template <typename T>
        size_t PublishMulti(const T &data, const BML_ImcMessage *msg = nullptr) {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            return PublishMulti(&data, sizeof(T), msg);
        }

        /**
         * @brief Notify all topics (empty message)
         */
        size_t Notify() {
            return Publish(nullptr, 0);
        }

        size_t Count() const noexcept { return m_Topics.size(); }
        bool Empty() const noexcept { return m_Topics.empty(); }
        void Clear() { m_Topics.clear(); }

        const std::vector<Topic> &GetTopics() const noexcept { return m_Topics; }

    private:
        std::vector<Topic> m_Topics;
    };

    // ========================================================================
    // Event Emitter Pattern
    // ========================================================================

    /**
     * @brief Simple event emitter for the common case
     *
     * Combines publisher and subscriber management in one class.
     * Useful for creating event-driven components.
     *
     * @code
     * class MyComponent {
     *     EventEmitter<GameEvent> onGameEvent{"Game/Event"};
     *
     * public:
     *     void FireEvent(const GameEvent& event) {
     *         onGameEvent.Emit(event);
     *     }
     * };
     * @endcode
     */
    template <typename T>
    class EventEmitter {
    public:
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

        EventEmitter() = default;

        explicit EventEmitter(std::string_view topicName)
            : m_Publisher(topicName) {}

        explicit EventEmitter(const Topic &topic)
            : m_Publisher(topic) {}

        bool Valid() const noexcept { return m_Publisher.Valid(); }
        explicit operator bool() const noexcept { return Valid(); }

        const Topic &GetTopic() const noexcept { return m_Publisher.GetTopic(); }

        /**
         * @brief Emit an event
         */
        bool Emit(const T &event) {
            return m_Publisher.Publish(event);
        }

        /**
         * @brief Emit with priority
         */
        bool Emit(const T &event, Priority prio) {
            return m_Publisher.Publish(event, prio);
        }

        /**
         * @brief Operator() for convenient emission
         */
        bool operator()(const T &event) {
            return Emit(event);
        }

        /**
         * @brief Get the underlying publisher
         */
        Publisher<T> &GetPublisher() { return m_Publisher; }
        const Publisher<T> &GetPublisher() const { return m_Publisher; }

    private:
        Publisher<T> m_Publisher;
    };

} // namespace imc
} // namespace bml

#endif /* BML_IMC_PUBLISHER_HPP */

