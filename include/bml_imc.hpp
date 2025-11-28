/**
 * @file bml_imc.hpp
 * @brief BML C++ Inter-Mod Communication Wrapper
 * 
 * Provides RAII-friendly, exception-safe IMC (Inter-Mod Communication) interface.
 */

#ifndef BML_IMC_HPP
#define BML_IMC_HPP

#include "bml_imc.h"
#include "bml_context.hpp"
#include "bml_errors.h"

#include <string>
#include <string_view>
#include <functional>
#include <memory>
#include <optional>

namespace bml {
    // ============================================================================
    // IMC Capabilities Query
    // ============================================================================

    /**
     * @brief Query IMC subsystem capabilities
     * @return Capabilities if successful
     */
    inline std::optional<BML_ImcCaps> GetImcCaps() {
        if (!bmlGetImcCaps) return std::nullopt;
        BML_ImcCaps caps = BML_IMC_CAPS_INIT;
        if (bmlGetImcCaps(&caps) == BML_RESULT_OK) {
            return caps;
        }
        return std::nullopt;
    }

    /**
     * @brief Check if an IMC capability is available
     * @param flag Capability flag to check
     * @return true if capability is available
     */
    inline bool HasImcCap(BML_ImcCapFlags flag) {
        auto caps = GetImcCaps();
        return caps && (caps->capability_flags & flag);
    }

    /**
     * @brief Get IMC global statistics
     * @return Statistics if available
     */
    inline std::optional<BML_ImcStats> GetImcStats() {
        if (!bmlImcGetStats) return std::nullopt;
        BML_ImcStats stats = BML_IMC_STATS_INIT;
        if (bmlImcGetStats(&stats) == BML_RESULT_OK) {
            return stats;
        }
        return std::nullopt;
    }

    // ============================================================================
    // IMC Callback Type
    // ============================================================================

    /**
     * @brief Callback function type for IMC subscriptions
     * @param data Pointer to message data
     * @param size Size of message data in bytes
     */
    using ImcCallback = std::function<void(const void *data, size_t size)>;

    // ============================================================================
    // IMC Implementation Details
    // ============================================================================

    namespace detail {
        /**
         * @internal
         * @brief Wrapper to bridge C callback to C++ std::function
         */
        struct ImcCallbackWrapper {
            ImcCallback callback;

            static void Invoke(BML_Context ctx, BML_TopicId topic, const BML_ImcMessage *msg, void *user_data) {
                (void) ctx;
                (void) topic;
                auto *wrapper = static_cast<ImcCallbackWrapper *>(user_data);
                if (wrapper && wrapper->callback && msg) {
                    wrapper->callback(msg->data, msg->size);
                }
            }
        };
    } // namespace detail

    // ============================================================================
    // IMC Wrapper
    // ============================================================================

    /**
     * @brief RAII wrapper for BML Inter-Mod Communication
     *
     * Example:
     *   bml::Imc imc(ctx);
     *
     *   // Publish an event
     *   imc.Publish("my_event", &data, sizeof(data));
     *
     *   // Subscribe with RAII handle
     *   auto subscription = imc.Subscribe("other_event", [](const void* data, size_t size) {
     *       // Handle event
     *   });
     */
    class Imc {
    public:
        /**
         * @brief Construct IMC wrapper
         * @param ctx The BML context
         */
        explicit Imc(Context ctx) : m_ctx(ctx.handle()) {}

        // ========================================================================
        // Publishing
        // ========================================================================

        /**
         * @brief Publish raw data to an event topic
         * @param event_name Event topic name
         * @param data Pointer to data (can be nullptr)
         * @param data_size Size of data in bytes
         * @return true if successful
         */
        bool Publish(std::string_view event_name, const void *data = nullptr, size_t data_size = 0) {
            if (!bmlImcPublish || !bmlImcGetTopicId) return false;
            BML_TopicId topic_id = 0;
            if (bmlImcGetTopicId(event_name.data(), &topic_id) != BML_RESULT_OK) return false;
            return bmlImcPublish(topic_id, data, data_size, nullptr) == BML_RESULT_OK;
        }

        /**
         * @brief Publish typed data to an event topic
         * @tparam T Data type (must be trivially copyable)
         * @param event_name Event topic name
         * @param data Data to publish
         * @return true if successful
         */
        template <typename T>
        bool Publish(std::string_view event_name, const T &data) {
            return Publish(event_name, &data, sizeof(T));
        }

        // ========================================================================
        // Subscription (RAII)
        // ========================================================================

        /**
         * @brief RAII subscription handle
         *
         * Automatically unsubscribes when destroyed. Non-copyable, movable.
         */
        class Subscription {
        public:
            Subscription() : m_ctx(nullptr), m_wrapper(nullptr), m_handle(nullptr), m_topic_id(0) {}

            /**
             * @brief Create a subscription
             * @param ctx The BML context
             * @param event_name Event topic name
             * @param callback Callback function
             * @throws bml::Exception if subscription fails
             */
            Subscription(BML_Context ctx, std::string_view event_name, ImcCallback callback)
                : m_ctx(ctx),
                  m_event_name(event_name),
                  m_wrapper(std::make_unique<detail::ImcCallbackWrapper>()),
                  m_handle(nullptr),
                  m_topic_id(0) {
                if (!bmlImcSubscribe || !bmlImcUnsubscribe || !bmlImcGetTopicId) {
                    throw Exception(BML_RESULT_NOT_FOUND, "IMC subscription API unavailable");
                }

                m_wrapper->callback = std::move(callback);

                if (bmlImcGetTopicId(m_event_name.c_str(), &m_topic_id) != BML_RESULT_OK) {
                    m_wrapper.reset();
                    throw Exception(BML_RESULT_NOT_FOUND, "Failed to get topic ID");
                }

                auto result = bmlImcSubscribe(m_topic_id,
                                              detail::ImcCallbackWrapper::Invoke,
                                              m_wrapper.get(),
                                              &m_handle);
                if (result != BML_RESULT_OK) {
                    m_wrapper.reset();
                    throw Exception(result, "Failed to subscribe to IMC event");
                }
            }

            /**
             * @brief Destructor - automatically unsubscribes
             */
            ~Subscription() {
                if (m_handle && bmlImcUnsubscribe) {
                    bmlImcUnsubscribe(m_handle);
                }
            }

            // Non-copyable
            Subscription(const Subscription &) = delete;
            Subscription &operator=(const Subscription &) = delete;

            // Movable
            Subscription(Subscription &&other) noexcept
                : m_ctx(other.m_ctx),
                  m_event_name(std::move(other.m_event_name)),
                  m_wrapper(std::move(other.m_wrapper)),
                  m_handle(other.m_handle),
                  m_topic_id(other.m_topic_id) {
                other.m_ctx = nullptr;
                other.m_handle = nullptr;
                other.m_topic_id = 0;
            }

            Subscription &operator=(Subscription &&other) noexcept {
                if (this != &other) {
                    if (m_handle && bmlImcUnsubscribe) {
                        bmlImcUnsubscribe(m_handle);
                    }
                    m_ctx = other.m_ctx;
                    m_event_name = std::move(other.m_event_name);
                    m_wrapper = std::move(other.m_wrapper);
                    m_handle = other.m_handle;
                    m_topic_id = other.m_topic_id;
                    other.m_ctx = nullptr;
                    other.m_handle = nullptr;
                    other.m_topic_id = 0;
                }
                return *this;
            }

            /**
             * @brief Check if subscription is valid
             */
            explicit operator bool() const noexcept { return m_handle != nullptr; }

            /**
             * @brief Get the topic ID
             */
            BML_TopicId topicId() const noexcept { return m_topic_id; }

            /**
             * @brief Get the event name
             */
            const std::string &eventName() const noexcept { return m_event_name; }

        private:
            BML_Context m_ctx;
            std::string m_event_name;
            std::unique_ptr<detail::ImcCallbackWrapper> m_wrapper;
            BML_Subscription m_handle;
            BML_TopicId m_topic_id;
        };

        /**
         * @brief Subscribe to an event topic
         * @param event_name Event topic name
         * @param callback Callback function
         * @return RAII subscription handle
         * @throws bml::Exception if subscription fails
         */
        Subscription Subscribe(std::string_view event_name, ImcCallback callback) {
            return Subscription(m_ctx, event_name, std::move(callback));
        }

    private:
        BML_Context m_ctx;
    };
} // namespace bml

#endif /* BML_IMC_HPP */
