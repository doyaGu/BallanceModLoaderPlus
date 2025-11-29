/**
 * @file bml_imc_message.hpp
 * @brief Message types and builders for BML IMC
 *
 * Provides type-safe, ergonomic message construction and access.
 */

#ifndef BML_IMC_MESSAGE_HPP
#define BML_IMC_MESSAGE_HPP

#include "bml_imc_fwd.hpp"
#include "bml_errors.h"

#include <algorithm>
#include <cstring>
#include <type_traits>
#include <vector>

namespace bml {
namespace imc {

    // ========================================================================
    // Message View (Non-owning)
    // ========================================================================

    /**
     * @brief Immutable view of an IMC message
     *
     * Non-owning wrapper around BML_ImcMessage for safe access to message data.
     * Does not copy data - only provides a view.
     *
     * @code
     * void OnMessage(const Message& msg) {
     *     auto data = msg.As<MyData>();  // Zero-copy typed access
     *     std::cout << "Priority: " << msg.Priority() << "\n";
     * }
     * @endcode
     */
    class Message {
    public:
        /** @brief Construct from C message struct */
        explicit Message(const BML_ImcMessage *msg) : m_Msg(msg) {}

        /** @brief Construct from C message struct reference */
        explicit Message(const BML_ImcMessage &msg) : m_Msg(&msg) {}

        // ====================================================================
        // Data Access
        // ====================================================================

        /** @brief Get raw data pointer */
        const void *Data() const noexcept { return m_Msg ? m_Msg->data : nullptr; }

        /** @brief Get data size in bytes */
        size_t Size() const noexcept { return m_Msg ? m_Msg->size : 0; }

        /** @brief Check if message is empty */
        bool Empty() const noexcept { return Size() == 0; }

        /** @brief Check if message is valid */
        bool Valid() const noexcept { return m_Msg != nullptr; }

        explicit operator bool() const noexcept { return Valid(); }

        /**
         * @brief Get typed view of data (zero-copy)
         * @tparam T Type to interpret data as (must be trivially copyable)
         * @return Pointer to typed data, nullptr if size mismatch
         */
        template <typename T>
        const T *As() const noexcept {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            if (Size() >= sizeof(T)) {
                return static_cast<const T *>(Data());
            }
            return nullptr;
        }

        /**
         * @brief Copy data to typed value
         * @tparam T Type to copy to
         * @param out Output value
         * @return true if successful
         */
        template <typename T>
        bool CopyTo(T &out) const noexcept {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            if (Size() >= sizeof(T) && Data()) {
                std::memcpy(&out, Data(), sizeof(T));
                return true;
            }
            return false;
        }

        /**
         * @brief Get data as string_view
         */
        std::string_view AsStringView() const noexcept {
            if (Data() && Size() > 0) {
                return std::string_view(static_cast<const char *>(Data()), Size());
            }
            return {};
        }

        /**
         * @brief Get data as string (copies data)
         */
        std::string AsString() const {
            return std::string(AsStringView());
        }

        /**
         * @brief Get data as byte vector (copies data)
         */
        std::vector<uint8_t> Bytes() const {
            if (Data() && Size() > 0) {
                return std::vector<uint8_t>(
                    static_cast<const uint8_t *>(Data()),
                    static_cast<const uint8_t *>(Data()) + Size()
                );
            }
            return {};
        }

        // ====================================================================
        // Metadata Access
        // ====================================================================

        /** @brief Get message ID */
        uint64_t Id() const noexcept { return m_Msg ? m_Msg->msg_id : 0; }

        /** @brief Get message flags */
        uint32_t Flags() const noexcept { return m_Msg ? m_Msg->flags : 0; }

        /** @brief Get message priority */
        Priority GetPriority() const noexcept {
            return m_Msg ? static_cast<Priority>(m_Msg->priority) : priority::Normal;
        }

        /** @brief Get message timestamp */
        uint64_t Timestamp() const noexcept { return m_Msg ? m_Msg->timestamp : 0; }

        /** @brief Get reply topic (for request/response patterns) */
        TopicId ReplyTopic() const noexcept { return m_Msg ? m_Msg->reply_topic : InvalidTopicId; }

        // ====================================================================
        // Flag Checks
        // ====================================================================

        bool HasFlag(uint32_t flag) const noexcept { return (Flags() & flag) != 0; }
        bool IsNoCopy() const noexcept { return HasFlag(flags::NoCopy); }
        bool IsBroadcast() const noexcept { return HasFlag(flags::Broadcast); }
        bool IsReliable() const noexcept { return HasFlag(flags::Reliable); }
        bool IsOrdered() const noexcept { return HasFlag(flags::Ordered); }
        bool IsCompressed() const noexcept { return HasFlag(flags::Compressed); }

        /** @brief Get underlying C struct */
        const BML_ImcMessage *Native() const noexcept { return m_Msg; }

    private:
        const BML_ImcMessage *m_Msg;
    };

    // ========================================================================
    // Message Builder (Fluent API)
    // ========================================================================

    /**
     * @brief Fluent builder for constructing IMC messages
     *
     * Provides a convenient way to build messages with various options.
     * Data is owned by the builder until publish.
     *
     * @code
     * // Simple usage
     * MessageBuilder()
     *     .data(&myData, sizeof(myData))
     *     .priority(priority::High)
     *     .Build();
     *
     * // With typed data
     * MessageBuilder()
     *     .Typed(myStruct)
     *     .flags(flags::Reliable | flags::Ordered)
     *     .Build();
     *
     * // With string
     * MessageBuilder()
     *     .String("Hello, World!")
     *     .Build();
     * @endcode
     */
    class MessageBuilder {
    public:
        MessageBuilder() {
            m_Msg = BML_IMC_MESSAGE_INIT;
        }

        // ====================================================================
        // Data Setters
        // ====================================================================

        /**
         * @brief Set raw data (non-owning, data must outlive publish)
         */
        MessageBuilder &Data(const void *ptr, size_t len) & {
            m_Msg.data = ptr;
            m_Msg.size = len;
            m_OwnedData.clear();
            return *this;
        }

        MessageBuilder &&Data(const void *ptr, size_t len) && {
            return std::move(Data(ptr, len));
        }

        /**
         * @brief Set typed data (non-owning)
         */
        template <typename T>
        MessageBuilder &Typed(const T &value) & {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            return Data(&value, sizeof(T));
        }

        template <typename T>
        MessageBuilder &&Typed(const T &value) && {
            return std::move(Typed(value));
        }

        /**
         * @brief Set data with ownership (copies data)
         */
        MessageBuilder &Copy(const void *ptr, size_t len) & {
            m_OwnedData.assign(
                static_cast<const uint8_t *>(ptr),
                static_cast<const uint8_t *>(ptr) + len
            );
            m_Msg.data = m_OwnedData.data();
            m_Msg.size = m_OwnedData.size();
            return *this;
        }

        MessageBuilder &&Copy(const void *ptr, size_t len) && {
            return std::move(Copy(ptr, len));
        }

        /**
         * @brief Set data from string (copies)
         */
        MessageBuilder &String(std::string_view str) & {
            return Copy(str.data(), str.size());
        }

        MessageBuilder &&String(std::string_view str) && {
            return std::move(String(str));
        }

        /**
         * @brief Set data from byte vector (moves ownership)
         */
        MessageBuilder &Bytes(std::vector<uint8_t> data) & {
            m_OwnedData = std::move(data);
            m_Msg.data = m_OwnedData.data();
            m_Msg.size = m_OwnedData.size();
            return *this;
        }

        MessageBuilder &&Bytes(std::vector<uint8_t> data) && {
            return std::move(Bytes(std::move(data)));
        }

        // ====================================================================
        // Metadata Setters
        // ====================================================================

        MessageBuilder &SetPriority(Priority p) & {
            m_Msg.priority = p;
            return *this;
        }

        MessageBuilder &&SetPriority(Priority p) && {
            return std::move(SetPriority(p));
        }

        MessageBuilder &SetFlags(uint32_t f) & {
            m_Msg.flags = f;
            return *this;
        }

        MessageBuilder &&SetFlags(uint32_t f) && {
            return std::move(SetFlags(f));
        }

        MessageBuilder &AddFlags(uint32_t f) & {
            m_Msg.flags |= f;
            return *this;
        }

        MessageBuilder &&AddFlags(uint32_t f) && {
            return std::move(AddFlags(f));
        }

        MessageBuilder &SetId(uint64_t msgId) & {
            m_Msg.msg_id = msgId;
            return *this;
        }

        MessageBuilder &&SetId(uint64_t msgId) && {
            return std::move(SetId(msgId));
        }

        MessageBuilder &SetTimestamp(uint64_t ts) & {
            m_Msg.timestamp = ts;
            return *this;
        }

        MessageBuilder &&SetTimestamp(uint64_t ts) && {
            return std::move(SetTimestamp(ts));
        }

        MessageBuilder &ReplyTo(TopicId topic) & {
            m_Msg.reply_topic = topic;
            return *this;
        }

        MessageBuilder &&ReplyTo(TopicId topic) && {
            return std::move(ReplyTo(topic));
        }

        // ====================================================================
        // Convenience Priority Setters
        // ====================================================================

        MessageBuilder &Low() & { return SetPriority(priority::Low); }
        MessageBuilder &&Low() && { return std::move(Low()); }

        MessageBuilder &Normal() & { return SetPriority(priority::Normal); }
        MessageBuilder &&Normal() && { return std::move(Normal()); }

        MessageBuilder &High() & { return SetPriority(priority::High); }
        MessageBuilder &&High() && { return std::move(High()); }

        MessageBuilder &Urgent() & { return SetPriority(priority::Urgent); }
        MessageBuilder &&Urgent() && { return std::move(Urgent()); }

        // ====================================================================
        // Build
        // ====================================================================

        /**
         * @brief Build the message struct
         * @return C message struct (data pointer valid only while builder exists)
         */
        const BML_ImcMessage &Build() const & noexcept {
            return m_Msg;
        }

        /**
         * @brief Get the native message struct
         */
        const BML_ImcMessage *Native() const noexcept {
            return &m_Msg;
        }

        /**
         * @brief Extract owned data (invalidates builder)
         */
        std::vector<uint8_t> ExtractData() && {
            m_Msg.data = nullptr;
            m_Msg.size = 0;
            return std::move(m_OwnedData);
        }

    private:
        BML_ImcMessage m_Msg;
        std::vector<uint8_t> m_OwnedData;
    };

    // ========================================================================
    // Zero-Copy Buffer
    // ========================================================================

    /**
     * @brief RAII wrapper for zero-copy IMC buffers
     *
     * Use for large payloads to avoid copying. The buffer automatically
     * cleans up when all subscribers have processed the message.
     *
     * @code
     * // Create buffer with custom cleanup
     * auto buffer = ZeroCopyBuffer::create(large_data, size,
     *     [](const void* data, size_t, void*) {
     *         delete[] static_cast<const char*>(data);
     *     });
     *
     * // Create buffer from vector (takes ownership)
     * auto buffer = ZeroCopyBuffer::fromVector(std::move(my_vector));
     * @endcode
     */
    class ZeroCopyBuffer {
    public:
        using CleanupFn = void (*)(const void *data, size_t size, void *userData);

        ZeroCopyBuffer() {
            m_Buffer = BML_IMC_BUFFER_INIT;
        }

        /**
         * @brief Create buffer with custom cleanup
         */
        static ZeroCopyBuffer Create(
            const void *data,
            size_t size,
            CleanupFn cleanup = nullptr,
            void *userData = nullptr
        ) {
            ZeroCopyBuffer buf;
            buf.m_Buffer.data = data;
            buf.m_Buffer.size = size;
            buf.m_Buffer.cleanup = cleanup;
            buf.m_Buffer.cleanup_user_data = userData;
            return buf;
        }

        /**
         * @brief Create buffer from vector (takes ownership)
         */
        static ZeroCopyBuffer FromVector(std::vector<uint8_t> &&data) {
            auto *owned = new std::vector<uint8_t>(std::move(data));
            return Create(
                owned->data(),
                owned->size(),
                [](const void *, size_t, void *ud) {
                    delete static_cast<std::vector<uint8_t> *>(ud);
                },
                owned
            );
        }

        /**
         * @brief Create buffer from string (takes ownership)
         */
        static ZeroCopyBuffer FromString(std::string &&data) {
            auto *owned = new std::string(std::move(data));
            return Create(
                owned->data(),
                owned->size(),
                [](const void *, size_t, void *ud) {
                    delete static_cast<std::string *>(ud);
                },
                owned
            );
        }

        const void *Data() const noexcept { return m_Buffer.data; }
        size_t Size() const noexcept { return m_Buffer.size; }
        const BML_ImcBuffer *Native() const noexcept { return &m_Buffer; }

    private:
        BML_ImcBuffer m_Buffer;
    };

    // ========================================================================
    // Callback Types (require Message class)
    // ========================================================================

    /**
     * @brief Full message callback with metadata
     */
    using MessageCallback = std::function<void(const Message &msg)>;

    /**
     * @brief RPC handler callback
     * @param request Request message
     * @return Response data (vector of bytes)
     */
    using RpcHandler = std::function<std::vector<uint8_t>(const Message &request)>;

    /**
     * @brief Typed RPC handler
     * @tparam Req Request type
     * @tparam Resp Response type
     */
    template <typename Req, typename Resp>
    using TypedRpcHandler = std::function<Resp(const Req &request)>;

    /**
     * @brief Message filter predicate (full message version)
     */
    using MessageFilterPredicate = std::function<bool(const Message &msg)>;

} // namespace imc
} // namespace bml

#endif /* BML_IMC_MESSAGE_HPP */

