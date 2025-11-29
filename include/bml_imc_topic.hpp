/**
 * @file bml_imc_topic.hpp
 * @brief Topic management for BML IMC
 *
 * Provides topic ID resolution, caching, and topic-centric publish/subscribe.
 */

#ifndef BML_IMC_TOPIC_HPP
#define BML_IMC_TOPIC_HPP

#include "bml_imc_fwd.hpp"
#include "bml_imc_message.hpp"
#include "bml_errors.h"

#include <optional>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>

namespace bml {
namespace imc {

    // ========================================================================
    // Topic Handle
    // ========================================================================

    /**
     * @brief Represents an IMC topic for pub/sub messaging
     *
     * Topics are identified by string names internally but use integer IDs
     * for fast publish/subscribe operations. This class caches the ID lookup.
     *
     * @code
     * // Create a topic (caches ID)
     * Topic physicsTick("Physics/Tick");
     *
     * // Check validity
     * if (physicsTick) {
     *     physicsTick.Publish(deltaTime);
     * }
     *
     * // Get topic info
     * auto info = physicsTick.Info();
     * @endcode
     */
    class Topic {
    public:
        /**
         * @brief Default constructor (invalid topic)
         */
        Topic() : m_Id(InvalidTopicId) {}

        /**
         * @brief Construct from topic name
         * @param name Topic name (e.g., "MyMod/Events/Update")
         */
        explicit Topic(std::string_view name)
            : m_Name(name), m_Id(InvalidTopicId) {
            Resolve();
        }

        /**
         * @brief Construct from pre-resolved ID
         * @param id Topic ID
         * @param name Optional name for debugging
         */
        explicit Topic(TopicId id, std::string name = "")
            : m_Name(std::move(name)), m_Id(id) {}

        // ====================================================================
        // Properties
        // ====================================================================

        /** @brief Get topic ID (0 if invalid) */
        TopicId Id() const noexcept { return m_Id; }

        /** @brief Get topic name */
        const std::string &Name() const noexcept { return m_Name; }

        /** @brief Check if topic is valid */
        bool Valid() const noexcept { return m_Id != InvalidTopicId; }

        explicit operator bool() const noexcept { return Valid(); }

        /** @brief Implicit conversion to TopicId for C API compatibility */
        operator TopicId() const noexcept { return m_Id; }

        // ====================================================================
        // Resolution
        // ====================================================================

        /**
         * @brief Resolve topic name to ID
         * @return true if resolution succeeded
         */
        bool Resolve() {
            if (m_Id != InvalidTopicId) return true;
            if (m_Name.empty() || !bmlImcGetTopicId) return false;
            return bmlImcGetTopicId(m_Name.c_str(), &m_Id) == BML_RESULT_OK;
        }

        /**
         * @brief Create a topic, resolving the name
         * @param name Topic name
         * @return Topic if successful, nullopt otherwise
         */
        static std::optional<Topic> Create(std::string_view name) {
            Topic topic(name);
            if (topic.Valid()) {
                return topic;
            }
            return std::nullopt;
        }

        // ====================================================================
        // Publishing
        // ====================================================================

        /**
         * @brief Publish raw data to this topic
         * @return true if successful
         */
        bool Publish(const void *data = nullptr, size_t size = 0) const {
            if (!Valid() || !bmlImcPublish) return false;
            return bmlImcPublish(m_Id, data, size) == BML_RESULT_OK;
        }

        /**
         * @brief Publish typed data
         */
        template <typename T>
        bool Publish(const T &data) const {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            return Publish(&data, sizeof(T));
        }

        /**
         * @brief Publish string data
         */
        bool PublishString(std::string_view str) const {
            return Publish(str.data(), str.size());
        }

        /**
         * @brief Publish with extended options
         */
        bool PublishEx(const BML_ImcMessage &msg) const {
            if (!Valid() || !bmlImcPublishEx) return false;
            return bmlImcPublishEx(m_Id, &msg) == BML_RESULT_OK;
        }

        /**
         * @brief Publish with message builder
         */
        bool PublishEx(const MessageBuilder &builder) const {
            return PublishEx(builder.Build());
        }

        /**
         * @brief Publish zero-copy buffer
         */
        bool PublishBuffer(const ZeroCopyBuffer &buffer) const {
            if (!Valid() || !bmlImcPublishBuffer) return false;
            return bmlImcPublishBuffer(m_Id, buffer.Native()) == BML_RESULT_OK;
        }

        // ====================================================================
        // Diagnostics
        // ====================================================================

        /**
         * @brief Get topic information
         */
        std::optional<BML_TopicInfo> Info() const {
            if (!Valid() || !bmlImcGetTopicInfo) return std::nullopt;
            BML_TopicInfo topicInfo = BML_TOPIC_INFO_INIT;
            if (bmlImcGetTopicInfo(m_Id, &topicInfo) == BML_RESULT_OK) {
                return topicInfo;
            }
            return std::nullopt;
        }

        /**
         * @brief Get subscriber count
         */
        size_t SubscriberCount() const {
            auto i = Info();
            return i ? i->subscriber_count : 0;
        }

        /**
         * @brief Get total message count
         */
        uint64_t MessageCount() const {
            auto i = Info();
            return i ? i->message_count : 0;
        }

        // ====================================================================
        // Comparison
        // ====================================================================

        bool operator==(const Topic &other) const noexcept { return m_Id == other.m_Id; }
        bool operator!=(const Topic &other) const noexcept { return m_Id != other.m_Id; }
        bool operator<(const Topic &other) const noexcept { return m_Id < other.m_Id; }

    private:
        std::string m_Name;
        TopicId m_Id;
    };

    // ========================================================================
    // Topic Registry (Thread-Safe Cache)
    // ========================================================================

    /**
     * @brief Thread-safe topic ID cache
     *
     * Caches topic name to ID mappings for fast repeated lookups.
     * Useful when you have many topics or dynamic topic names.
     *
     * @code
     * TopicRegistry registry;
     *
     * // Get or create topic (cached)
     * Topic topic = registry.Get("MyMod/Events/Update");
     *
     * // Publish through registry
     * registry.Publish("MyMod/Events/Update", &data, sizeof(data));
     * @endcode
     */
    class TopicRegistry {
    public:
        /**
         * @brief Get or create a topic by name
         * @param name Topic name
         * @return Topic (may be invalid if resolution fails)
         */
        Topic Get(std::string_view name) {
            std::string key(name);

            // Fast path: read lock
            {
                std::shared_lock lock(m_Mutex);
                auto it = m_Topics.find(key);
                if (it != m_Topics.end()) {
                    return it->second;
                }
            }

            // Slow path: write lock
            std::unique_lock lock(m_Mutex);
            auto it = m_Topics.find(key);
            if (it != m_Topics.end()) {
                return it->second;
            }

            Topic topic(name);
            m_Topics.emplace(std::move(key), topic);
            return topic;
        }

        /**
         * @brief Get topic if it exists in cache
         */
        std::optional<Topic> Find(std::string_view name) const {
            std::shared_lock lock(m_Mutex);
            auto it = m_Topics.find(std::string(name));
            if (it != m_Topics.end()) {
                return it->second;
            }
            return std::nullopt;
        }

        /**
         * @brief Publish to a topic by name
         */
        bool Publish(std::string_view name, const void *data = nullptr, size_t size = 0) {
            return Get(name).Publish(data, size);
        }

        /**
         * @brief Publish typed data to a topic by name
         */
        template <typename T>
        bool Publish(std::string_view name, const T &data) {
            return Get(name).Publish(data);
        }

        /**
         * @brief Clear the cache
         */
        void Clear() {
            std::unique_lock lock(m_Mutex);
            m_Topics.clear();
        }

        /**
         * @brief Get number of cached topics
         */
        size_t Size() const {
            std::shared_lock lock(m_Mutex);
            return m_Topics.size();
        }

    private:
        mutable std::shared_mutex m_Mutex;
        std::unordered_map<std::string, Topic> m_Topics;
    };

    /**
     * @brief Get the global topic registry
     */
    inline TopicRegistry &GlobalTopicRegistry() {
        static TopicRegistry registry;
        return registry;
    }

    /**
     * @brief Quick topic lookup via global registry
     */
    inline Topic GetTopic(std::string_view name) {
        return GlobalTopicRegistry().Get(name);
    }

} // namespace imc
} // namespace bml

// ============================================================================
// std::hash specialization
// ============================================================================

namespace std {

    template <>
    struct hash<bml::imc::Topic> {
        size_t operator()(const bml::imc::Topic &topic) const noexcept {
            return std::hash<bml::imc::TopicId>{}(topic.Id());
        }
    };

} // namespace std

#endif /* BML_IMC_TOPIC_HPP */

