/**
 * @file bml_imc_topic.hpp
 * @brief Topic management for BML IMC
 */

#ifndef BML_IMC_TOPIC_HPP
#define BML_IMC_TOPIC_HPP

#include "bml_imc_fwd.hpp"
#include "bml_imc_message.hpp"

#include <optional>
#include <string>

namespace bml {
namespace imc {

    /**
     * @brief Represents an IMC topic for pub/sub messaging
     *
     * Caches the integer ID after first resolution for fast repeated use.
     */
    class Topic {
    public:
        Topic() : m_Id(InvalidTopicId) {}

        explicit Topic(std::string_view name,
                       const BML_ImcBusInterface *bus = nullptr,
                       BML_Mod owner = nullptr)
            : m_Name(name), m_Id(InvalidTopicId), m_Bus(bus), m_Owner(owner) {
            Resolve();
        }

        explicit Topic(TopicId id,
                       std::string name = "",
                       const BML_ImcBusInterface *bus = nullptr,
                       BML_Mod owner = nullptr)
            : m_Name(std::move(name)), m_Id(id), m_Bus(bus), m_Owner(owner) {}

        TopicId Id() const noexcept { return m_Id; }
        const std::string &Name() const noexcept { return m_Name; }
        const BML_ImcBusInterface *Iface() const noexcept { return m_Bus; }
        bool Valid() const noexcept { return m_Id != InvalidTopicId; }
        explicit operator bool() const noexcept { return Valid(); }
        operator TopicId() const noexcept { return m_Id; }

        bool Resolve() {
            if (m_Id != InvalidTopicId) return true;
            if (m_Name.empty() || !m_Bus || !m_Bus->Context || !m_Bus->GetTopicId) return false;
            return m_Bus->GetTopicId(m_Bus->Context, m_Name.c_str(), &m_Id) == BML_RESULT_OK;
        }

        static std::optional<Topic> Create(std::string_view name, const BML_ImcBusInterface *bus = nullptr) {
            Topic topic(name, bus);
            if (topic.Valid()) return topic;
            return std::nullopt;
        }

        // Publishing
        bool Publish(const void *data = nullptr, size_t size = 0) const {
            if (!Valid() || !m_Bus || !m_Owner || !m_Bus->Publish) return false;
            return m_Bus->Publish(m_Owner, m_Id, data, size) == BML_RESULT_OK;
        }

        template <typename T>
        bool Publish(const T &data) const {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            return Publish(&data, sizeof(T));
        }

        bool PublishString(std::string_view str) const {
            return Publish(str.data(), str.size());
        }

        bool PublishEx(const BML_ImcMessage &msg) const {
            if (!Valid() || !m_Bus || !m_Owner || !m_Bus->PublishEx) return false;
            return m_Bus->PublishEx(m_Owner, m_Id, &msg) == BML_RESULT_OK;
        }

        bool PublishEx(const MessageBuilder &builder) const {
            return PublishEx(builder.Build());
        }

        bool PublishBuffer(const ZeroCopyBuffer &buffer) const {
            if (!Valid() || !m_Bus || !m_Owner || !m_Bus->PublishBuffer) return false;
            return m_Bus->PublishBuffer(m_Owner, m_Id, buffer.Native()) == BML_RESULT_OK;
        }

        // Retained State
        bool PublishState(const void *data, size_t size) const {
            if (!Valid() || !m_Bus || !m_Owner || !m_Bus->PublishState) return false;
            BML_ImcMessage msg = BML_IMC_MESSAGE_INIT;
            msg.data = data;
            msg.size = size;
            return m_Bus->PublishState(m_Owner, m_Id, &msg) == BML_RESULT_OK;
        }

        template <typename T>
        bool PublishState(const T &data) const {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            return PublishState(&data, sizeof(T));
        }

        BML_Result CopyState(void *dst, size_t dst_size, size_t *out_size = nullptr,
                              BML_ImcStateMeta *out_meta = nullptr) const {
            if (!Valid() || !m_Bus || !m_Bus->Context || !m_Bus->CopyState)
                return BML_RESULT_NOT_SUPPORTED;
            return m_Bus->CopyState(m_Bus->Context, m_Id, dst, dst_size, out_size, out_meta);
        }

        bool ClearState() const {
            if (!Valid() || !m_Bus || !m_Bus->Context || !m_Bus->ClearState) return false;
            return m_Bus->ClearState(m_Bus->Context, m_Id) == BML_RESULT_OK;
        }

        // Interceptable Publishing
        bool PublishInterceptable(BML_ImcMessage &msg, EventResult *outResult = nullptr) const {
            if (!Valid() || !m_Bus || !m_Owner || !m_Bus->PublishInterceptable) return false;
            return m_Bus->PublishInterceptable(m_Owner, m_Id, &msg, outResult) == BML_RESULT_OK;
        }

        template <typename T>
        bool PublishInterceptable(const T &data, EventResult *outResult = nullptr) const {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            BML_ImcMessage msg = BML_IMC_MSG(&data, sizeof(T));
            return PublishInterceptable(msg, outResult);
        }

        // Diagnostics
        std::optional<BML_TopicInfo> Info() const {
            if (!Valid() || !m_Bus || !m_Bus->Context || !m_Bus->GetTopicInfo) return std::nullopt;
            BML_TopicInfo info = BML_TOPIC_INFO_INIT;
            if (m_Bus->GetTopicInfo(m_Bus->Context, m_Id, &info) == BML_RESULT_OK) return info;
            return std::nullopt;
        }

        size_t SubscriberCount() const { auto i = Info(); return i ? i->subscriber_count : 0; }
        uint64_t MessageCount() const { auto i = Info(); return i ? i->message_count : 0; }

        bool operator==(const Topic &other) const noexcept { return m_Id == other.m_Id; }
        bool operator!=(const Topic &other) const noexcept { return m_Id != other.m_Id; }
        bool operator<(const Topic &other) const noexcept { return m_Id < other.m_Id; }

    private:
        std::string m_Name;
        TopicId m_Id;
        const BML_ImcBusInterface *m_Bus = nullptr;
        BML_Mod m_Owner = nullptr;
    };

} // namespace imc
} // namespace bml

namespace std {
    template <>
    struct hash<bml::imc::Topic> {
        size_t operator()(const bml::imc::Topic &topic) const noexcept {
            return std::hash<bml::imc::TopicId>{}(topic.Id());
        }
    };
}

#endif /* BML_IMC_TOPIC_HPP */
