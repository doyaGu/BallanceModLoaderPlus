/**
 * @file bml_imc_typed.hpp
 * @brief Compile-time type-safe IMC messaging: PayloadType<T> and TypedTopic<T>
 */

#ifndef BML_IMC_TYPED_HPP
#define BML_IMC_TYPED_HPP

#include "bml_imc_fwd.hpp"
#include "bml_imc_topic.hpp"

#include <type_traits>

namespace bml {
namespace imc {

    // PayloadType<T> and PayloadType<void> are defined in bml_imc_fwd.hpp

    // ========================================================================
    // TypedTopic<T> -- type-safe topic wrapper
    // ========================================================================

    /**
     * @brief A topic that enforces a specific payload type.
     *
     * Wraps bml::imc::Topic and automatically provides the correct
     * payload_type_id on every Publish call.
     *
     * @tparam T  The payload struct type, or void for empty payloads.
     */
    template <typename T>
    class TypedTopic {
    public:
        TypedTopic() = default;

        explicit TypedTopic(std::string_view name,
                            const BML_ImcBusInterface *bus = nullptr,
                            BML_Mod owner = nullptr)
            : m_Inner(name, bus, owner) {}

        /** @brief Publish a typed payload */
        bool Publish(const T &payload) const {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            if (!m_Inner.Valid()) return false;
            BML_ImcMessage msg = BML_IMC_MESSAGE_INIT;
            msg.data = &payload;
            msg.size = sizeof(T);
            msg.payload_type_id = PayloadType<T>::Id;
            return m_Inner.PublishEx(msg);
        }

        /** @brief Publish a typed payload as retained state */
        bool PublishState(const T &payload) const {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            if (!m_Inner.Valid() || !m_Inner.Iface() || !m_Inner.Iface()->PublishState)
                return false;
            BML_ImcMessage msg = BML_IMC_MESSAGE_INIT;
            msg.data = &payload;
            msg.size = sizeof(T);
            msg.payload_type_id = PayloadType<T>::Id;
            auto *bus = m_Inner.Iface();
            auto *owner_ptr = m_Inner.Owner();
            return bus->PublishState(owner_ptr, m_Inner.Id(), &msg) == BML_RESULT_OK;
        }

        /** @brief Publish a typed payload as an interceptable event */
        bool PublishInterceptable(const T &payload, EventResult *outResult = nullptr) const {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            BML_ImcMessage msg = BML_IMC_MSG(&payload, sizeof(T));
            msg.payload_type_id = PayloadType<T>::Id;
            return m_Inner.PublishInterceptable(msg, outResult);
        }

        /** @brief Publish a zero-copy buffer with type tag */
        bool PublishBuffer(const ZeroCopyBuffer &buffer) const {
            return m_Inner.PublishBuffer(buffer);
        }

        bool Valid() const noexcept { return m_Inner.Valid(); }
        explicit operator bool() const noexcept { return m_Inner.Valid(); }
        TopicId Id() const noexcept { return m_Inner.Id(); }
        const std::string &Name() const noexcept { return m_Inner.Name(); }

        /** @brief Access the underlying untyped Topic */
        const Topic &Inner() const noexcept { return m_Inner; }
        Topic &Inner() noexcept { return m_Inner; }

        /** @brief Get the compile-time type ID for this topic's payload */
        static constexpr uint32_t TypeId() noexcept { return PayloadType<T>::Id; }

        size_t SubscriberCount() const { return m_Inner.SubscriberCount(); }
        uint64_t MessageCount() const { return m_Inner.MessageCount(); }

        bool ClearState() const { return m_Inner.ClearState(); }
        BML_Result CopyState(void *dst, size_t dst_size, size_t *out_size = nullptr,
                              BML_ImcStateMeta *out_meta = nullptr) const {
            return m_Inner.CopyState(dst, dst_size, out_size, out_meta);
        }

    private:
        Topic m_Inner;
    };

    /**
     * @brief Specialization for void -- topics with no payload.
     */
    template <>
    class TypedTopic<void> {
    public:
        TypedTopic() = default;

        explicit TypedTopic(std::string_view name,
                            const BML_ImcBusInterface *bus = nullptr,
                            BML_Mod owner = nullptr)
            : m_Inner(name, bus, owner) {}

        /** @brief Publish with no payload */
        bool Publish() const {
            if (!m_Inner.Valid()) return false;
            BML_ImcMessage msg = BML_IMC_MESSAGE_INIT;
            msg.payload_type_id = BML_PAYLOAD_TYPE_NONE;
            return m_Inner.PublishEx(msg);
        }

        bool Valid() const noexcept { return m_Inner.Valid(); }
        explicit operator bool() const noexcept { return m_Inner.Valid(); }
        TopicId Id() const noexcept { return m_Inner.Id(); }
        const std::string &Name() const noexcept { return m_Inner.Name(); }

        const Topic &Inner() const noexcept { return m_Inner; }
        Topic &Inner() noexcept { return m_Inner; }

        static constexpr uint32_t TypeId() noexcept { return BML_PAYLOAD_TYPE_NONE; }

        size_t SubscriberCount() const { return m_Inner.SubscriberCount(); }
        uint64_t MessageCount() const { return m_Inner.MessageCount(); }

    private:
        Topic m_Inner;
    };

} // namespace imc
} // namespace bml

#endif /* BML_IMC_TYPED_HPP */
