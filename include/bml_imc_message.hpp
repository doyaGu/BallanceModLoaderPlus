/**
 * @file bml_imc_message.hpp
 * @brief Message types and builders for BML IMC
 */

#ifndef BML_IMC_MESSAGE_HPP
#define BML_IMC_MESSAGE_HPP

#include "bml_imc_fwd.hpp"

#include <algorithm>
#include <cstring>
#include <type_traits>
#include <vector>

namespace bml {
namespace imc {

    // ========================================================================
    // Message View (Non-owning)
    // ========================================================================

    class Message {
    public:
        explicit Message(const BML_ImcMessage *msg) : m_Msg(msg) {}
        explicit Message(const BML_ImcMessage &msg) : m_Msg(&msg) {}

        const void *Data() const noexcept { return m_Msg ? m_Msg->data : nullptr; }
        size_t Size() const noexcept { return m_Msg ? m_Msg->size : 0; }
        bool Empty() const noexcept { return Size() == 0; }
        bool Valid() const noexcept { return m_Msg != nullptr; }
        explicit operator bool() const noexcept { return Valid(); }

        template <typename T>
        const T *As() const noexcept {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            if (!m_Msg) return nullptr;
            // Type validation: if message has a type_id set, it must match T
            if (m_Msg->payload_type_id != BML_PAYLOAD_TYPE_NONE &&
                m_Msg->payload_type_id != PayloadType<T>::Id) {
                return nullptr;
            }
            if (Size() >= sizeof(T))
                return static_cast<const T *>(Data());
            return nullptr;
        }

        template <typename T>
        bool CopyTo(T &out) const noexcept {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            if (Size() >= sizeof(T) && Data()) {
                std::memcpy(&out, Data(), sizeof(T));
                return true;
            }
            return false;
        }

        std::string_view AsStringView() const noexcept {
            if (Data() && Size() > 0)
                return {static_cast<const char *>(Data()), Size()};
            return {};
        }

        std::string AsString() const { return std::string(AsStringView()); }

        std::vector<uint8_t> Bytes() const {
            if (Data() && Size() > 0) {
                auto *p = static_cast<const uint8_t *>(Data());
                return {p, p + Size()};
            }
            return {};
        }

        uint64_t Id() const noexcept { return m_Msg ? m_Msg->msg_id : 0; }
        uint32_t Flags() const noexcept { return m_Msg ? m_Msg->flags : 0; }
        Priority GetPriority() const noexcept {
            return m_Msg ? static_cast<Priority>(m_Msg->priority) : priority::Normal;
        }
        uint64_t Timestamp() const noexcept { return m_Msg ? m_Msg->timestamp : 0; }
        TopicId ReplyTopic() const noexcept { return m_Msg ? m_Msg->reply_topic : InvalidTopicId; }
        uint32_t PayloadTypeId() const noexcept {
            return m_Msg ? m_Msg->payload_type_id : BML_PAYLOAD_TYPE_NONE;
        }

        bool HasFlag(uint32_t flag) const noexcept { return (Flags() & flag) != 0; }

        const BML_ImcMessage *Native() const noexcept { return m_Msg; }

    private:
        const BML_ImcMessage *m_Msg;
    };

    // ========================================================================
    // Mutable Message View (for intercept handlers)
    // ========================================================================

    class MutableMessage {
    public:
        explicit MutableMessage(BML_ImcMessage *msg) : m_Msg(msg) {}

        const void *Data() const noexcept { return m_Msg ? m_Msg->data : nullptr; }
        size_t Size() const noexcept { return m_Msg ? m_Msg->size : 0; }
        bool Empty() const noexcept { return Size() == 0; }
        bool Valid() const noexcept { return m_Msg != nullptr; }
        explicit operator bool() const noexcept { return Valid(); }
        uint64_t Id() const noexcept { return m_Msg ? m_Msg->msg_id : 0; }
        uint32_t Flags() const noexcept { return m_Msg ? m_Msg->flags : 0; }
        Priority GetPriority() const noexcept {
            return m_Msg ? static_cast<Priority>(m_Msg->priority) : priority::Normal;
        }
        uint64_t Timestamp() const noexcept { return m_Msg ? m_Msg->timestamp : 0; }

        template <typename T>
        const T *As() const noexcept {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            if (!m_Msg) return nullptr;
            if (m_Msg->payload_type_id != BML_PAYLOAD_TYPE_NONE &&
                m_Msg->payload_type_id != PayloadType<T>::Id) {
                return nullptr;
            }
            return (Size() >= sizeof(T)) ? static_cast<const T *>(Data()) : nullptr;
        }

        std::string_view AsStringView() const noexcept {
            if (Data() && Size() > 0)
                return {static_cast<const char *>(Data()), Size()};
            return {};
        }

        void SetData(const void *data, size_t size) {
            if (m_Msg) { m_Msg->data = data; m_Msg->size = size; }
        }
        void SetFlags(uint32_t f) { if (m_Msg) m_Msg->flags = f; }
        void AddFlags(uint32_t f) { if (m_Msg) m_Msg->flags |= f; }
        void SetPriority(Priority p) { if (m_Msg) m_Msg->priority = p; }

        BML_ImcMessage *Native() noexcept { return m_Msg; }
        const BML_ImcMessage *Native() const noexcept { return m_Msg; }

    private:
        BML_ImcMessage *m_Msg;
    };

    // ========================================================================
    // Callback Types (require Message / MutableMessage)
    // ========================================================================

    using MessageCallback = std::function<void(const Message &msg)>;
    using MessageFilterPredicate = std::function<bool(const Message &msg)>;
    using InterceptCallback = std::function<EventResult(MutableMessage &msg)>;

    template <typename T>
    using TypedInterceptCallback = std::function<EventResult(const T &data)>;

    using RpcHandler = std::function<std::vector<uint8_t>(const Message &request)>;

    template <typename Req, typename Resp>
    using TypedRpcHandler = std::function<Resp(const Req &request)>;

    // ========================================================================
    // Message Builder (Fluent API)
    // ========================================================================

    class MessageBuilder {
    public:
        MessageBuilder() { m_Msg = BML_IMC_MESSAGE_INIT; }

        MessageBuilder &Data(const void *ptr, size_t len) {
            m_Msg.data = ptr;
            m_Msg.size = len;
            m_StorageData.clear();
            return *this;
        }

        template <typename T>
        MessageBuilder &Typed(const T &value) {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            return Data(&value, sizeof(T));
        }

        MessageBuilder &Copy(const void *ptr, size_t len) {
            m_StorageData.assign(
                static_cast<const uint8_t *>(ptr),
                static_cast<const uint8_t *>(ptr) + len);
            m_Msg.data = m_StorageData.data();
            m_Msg.size = m_StorageData.size();
            return *this;
        }

        MessageBuilder &String(std::string_view str) { return Copy(str.data(), str.size()); }

        MessageBuilder &Bytes(std::vector<uint8_t> data) {
            m_StorageData = std::move(data);
            m_Msg.data = m_StorageData.data();
            m_Msg.size = m_StorageData.size();
            return *this;
        }

        MessageBuilder &SetPriority(Priority p) { m_Msg.priority = p; return *this; }
        MessageBuilder &SetFlags(uint32_t f) { m_Msg.flags = f; return *this; }
        MessageBuilder &AddFlags(uint32_t f) { m_Msg.flags |= f; return *this; }
        MessageBuilder &SetId(uint64_t id) { m_Msg.msg_id = id; return *this; }
        MessageBuilder &SetTimestamp(uint64_t ts) { m_Msg.timestamp = ts; return *this; }
        MessageBuilder &ReplyTo(TopicId topic) { m_Msg.reply_topic = topic; return *this; }

        MessageBuilder &Low() { return SetPriority(priority::Low); }
        MessageBuilder &Normal() { return SetPriority(priority::Normal); }
        MessageBuilder &High() { return SetPriority(priority::High); }
        MessageBuilder &Urgent() { return SetPriority(priority::Urgent); }

        const BML_ImcMessage &Build() const & noexcept { return m_Msg; }
        const BML_ImcMessage *Native() const noexcept { return &m_Msg; }

        std::vector<uint8_t> ExtractData() && {
            m_Msg.data = nullptr;
            m_Msg.size = 0;
            return std::move(m_StorageData);
        }

    private:
        BML_ImcMessage m_Msg;
        std::vector<uint8_t> m_StorageData;
    };

    // ========================================================================
    // Zero-Copy Buffer
    // ========================================================================

    class ZeroCopyBuffer {
    public:
        using CleanupFn = void (*)(const void *data, size_t size, void *userData);

        ZeroCopyBuffer() { m_Buffer = BML_IMC_BUFFER_INIT; }

        static ZeroCopyBuffer Create(const void *data, size_t size,
                                     CleanupFn cleanup = nullptr, void *userData = nullptr) {
            ZeroCopyBuffer buf;
            buf.m_Buffer.data = data;
            buf.m_Buffer.size = size;
            buf.m_Buffer.cleanup = cleanup;
            buf.m_Buffer.cleanup_user_data = userData;
            return buf;
        }

        static ZeroCopyBuffer FromVector(std::vector<uint8_t> &&data) {
            auto *storage = new std::vector<uint8_t>(std::move(data));
            return Create(storage->data(), storage->size(),
                [](const void *, size_t, void *ud) { delete static_cast<std::vector<uint8_t> *>(ud); },
                storage);
        }

        static ZeroCopyBuffer FromString(std::string &&data) {
            auto *storage = new std::string(std::move(data));
            return Create(storage->data(), storage->size(),
                [](const void *, size_t, void *ud) { delete static_cast<std::string *>(ud); },
                storage);
        }

        const void *Data() const noexcept { return m_Buffer.data; }
        size_t Size() const noexcept { return m_Buffer.size; }
        const BML_ImcBuffer *Native() const noexcept { return &m_Buffer; }

    private:
        BML_ImcBuffer m_Buffer;
    };

} // namespace imc
} // namespace bml

#endif /* BML_IMC_MESSAGE_HPP */
