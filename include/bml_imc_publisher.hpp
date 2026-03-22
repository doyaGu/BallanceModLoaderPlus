/**
 * @file bml_imc_publisher.hpp
 * @brief Publisher abstraction for BML IMC
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

    template <typename T = void>
    class Publisher {
        static_assert(std::is_void_v<T> || std::is_trivially_copyable_v<T>,
                      "T must be trivially copyable or void");
    public:
        Publisher() = default;

        explicit Publisher(std::string_view topicName,
                           const BML_ImcBusInterface *bus = nullptr,
                           BML_Mod owner = nullptr)
            : m_Topic(topicName, bus, owner) {}

        explicit Publisher(const Topic &topic,
                           const BML_ImcBusInterface *bus = nullptr,
                           BML_Mod owner = nullptr)
            : m_Topic(topic.Valid()
                          ? Topic(topic.Id(), topic.Name(), bus ? bus : topic.Iface(), owner)
                          : Topic()) {}

        bool Valid() const noexcept { return m_Topic.Valid(); }
        explicit operator bool() const noexcept { return Valid(); }
        const Topic &GetTopic() const noexcept { return m_Topic; }
        TopicId GetTopicId() const noexcept { return m_Topic.Id(); }

        // Typed publish (only when T is not void)
        template <typename U = T, std::enable_if_t<!std::is_void_v<U>, int> = 0>
        bool Publish(const U &data) { return m_Topic.Publish(data); }

        template <typename U = T, std::enable_if_t<!std::is_void_v<U>, int> = 0>
        bool Publish(const U &data, Priority prio) {
            BML_ImcMessage msg = BML_IMC_MSG(&data, sizeof(U));
            msg.priority = prio;
            return m_Topic.PublishEx(msg);
        }

        template <typename U = T, std::enable_if_t<!std::is_void_v<U>, int> = 0>
        bool PublishHigh(const U &data) { return Publish(data, priority::High); }

        template <typename U = T, std::enable_if_t<!std::is_void_v<U>, int> = 0>
        bool PublishUrgent(const U &data) { return Publish(data, priority::Urgent); }

        // Untyped publish (available for all T including void)
        template <typename Any>
        bool PublishAny(const Any &data) {
            static_assert(std::is_trivially_copyable_v<Any>, "Must be trivially copyable");
            return m_Topic.Publish(data);
        }

        template <typename Any>
        bool PublishAny(const Any &data, Priority prio) {
            static_assert(std::is_trivially_copyable_v<Any>, "Must be trivially copyable");
            BML_ImcMessage msg = BML_IMC_MSG(&data, sizeof(Any));
            msg.priority = prio;
            return m_Topic.PublishEx(msg);
        }

        bool PublishRaw(const void *data = nullptr, size_t size = 0) { return m_Topic.Publish(data, size); }
        bool PublishString(std::string_view str) { return m_Topic.PublishString(str); }
        bool PublishEx(const MessageBuilder &builder) { return m_Topic.PublishEx(builder); }
        bool PublishEx(const BML_ImcMessage &msg) { return m_Topic.PublishEx(msg); }
        bool PublishBuffer(const ZeroCopyBuffer &buffer) { return m_Topic.PublishBuffer(buffer); }

        bool Notify() { return PublishRaw(); }
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

    class MultiPublisher {
    public:
        MultiPublisher() = default;

        explicit MultiPublisher(std::initializer_list<std::string_view> topicNames,
                                const BML_ImcBusInterface *bus = nullptr,
                                BML_Mod owner = nullptr)
            : m_Bus(bus), m_Owner(owner) {
            for (auto name : topicNames) Add(name);
        }

        bool Add(std::string_view name) {
            Topic topic(name, m_Bus, m_Owner);
            if (topic.Valid()) { m_Topics.push_back(topic); return true; }
            return false;
        }

        bool Add(const Topic &topic) {
            if (topic.Valid()) { m_Topics.push_back(topic); return true; }
            return false;
        }

        size_t Publish(const void *data = nullptr, size_t size = 0) {
            size_t count = 0;
            for (const auto &topic : m_Topics)
                if (topic.Publish(data, size)) ++count;
            return count;
        }

        template <typename T>
        size_t Publish(const T &data) {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            return Publish(&data, sizeof(T));
        }

        size_t PublishMulti(const void *data, size_t size, const BML_ImcMessage *msg = nullptr) {
            auto publishMulti = m_Bus ? m_Bus->PublishMulti : nullptr;
            if (m_Topics.empty() || !publishMulti || !m_Owner)
                return Publish(data, size);

            std::vector<TopicId> ids;
            ids.reserve(m_Topics.size());
            for (const auto &topic : m_Topics)
                ids.push_back(topic.Id());

            size_t delivered = 0;
            if (publishMulti(m_Owner, ids.data(), ids.size(), data, size, msg, &delivered) ==
                BML_RESULT_OK)
                return delivered;
            return 0;
        }

        size_t Notify() { return Publish(nullptr, 0); }
        size_t Count() const noexcept { return m_Topics.size(); }
        bool Empty() const noexcept { return m_Topics.empty(); }
        void Clear() { m_Topics.clear(); }
        const std::vector<Topic> &GetTopics() const noexcept { return m_Topics; }

    private:
        const BML_ImcBusInterface *m_Bus = nullptr;
        BML_Mod m_Owner = nullptr;
        std::vector<Topic> m_Topics;
    };

    // ========================================================================
    // Event Emitter
    // ========================================================================

    template <typename T>
    class EventEmitter {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    public:
        EventEmitter() = default;
        explicit EventEmitter(std::string_view topicName,
                              const BML_ImcBusInterface *bus = nullptr,
                              BML_Mod owner = nullptr)
            : m_Publisher(topicName, bus, owner) {}

        bool Valid() const noexcept { return m_Publisher.Valid(); }
        explicit operator bool() const noexcept { return Valid(); }
        const Topic &GetTopic() const noexcept { return m_Publisher.GetTopic(); }

        bool Emit(const T &event) { return m_Publisher.Publish(event); }
        bool Emit(const T &event, Priority prio) { return m_Publisher.Publish(event, prio); }
        bool operator()(const T &event) { return Emit(event); }

        Publisher<T> &GetPublisher() { return m_Publisher; }
        const Publisher<T> &GetPublisher() const { return m_Publisher; }

    private:
        Publisher<T> m_Publisher;
    };

} // namespace imc
} // namespace bml

#endif /* BML_IMC_PUBLISHER_HPP */
