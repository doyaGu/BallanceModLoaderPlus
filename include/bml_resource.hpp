/**
 * @file bml_resource.hpp
 * @brief BML C++ Resource Handle Wrapper
 *
 * Provides RAII-friendly and type-safe wrappers for BML resource handles.
 */

#ifndef BML_RESOURCE_HPP
#define BML_RESOURCE_HPP

#include "bml_resource.h"
#include "bml_errors.h"
#include "bml_assert.hpp"

#include <optional>
#include <memory>
#include <utility>

namespace bml {

    // ============================================================================
    // Handle Wrapper
    // ============================================================================

    /**
     * @brief RAII wrapper for BML resource handles
     *
     * Example:
     *   // Create a handle
     *   auto handle = bml::Handle::create(MY_RESOURCE_TYPE);
     *
     *   // Attach user data
     *   handle.AttachUserData(my_data);
     *
     *   // Handle is automatically released when it goes out of scope
     */
    class Handle {
    public:
        Handle() : m_valid(false) {
            m_desc = BML_HANDLE_DESC_INIT;
        }

        /**
         * @brief Create a new handle
         * @param type Handle type identifier
         * @return Handle instance
         * @throws bml::Exception if creation fails
         */
        static Handle create(BML_HandleType type,
                             const BML_CoreResourceInterface *resourceInterface = nullptr,
                             BML_Mod owner = nullptr) {
            BML_ASSERT(resourceInterface);
            BML_ASSERT(owner);

            Handle h;
            h.m_ResourceInterface = resourceInterface;
            const auto result = resourceInterface->HandleCreate(owner, type, &h.m_desc);
            if (result != BML_RESULT_OK) {
                throw Exception(result, "Failed to create handle");
            }
            h.m_valid = true;
            return h;
        }

        /**
         * @brief Create a handle without throwing
         * @param type Handle type identifier
         * @return Handle if successful
         */
        static std::optional<Handle> tryCreate(
            BML_HandleType type,
            const BML_CoreResourceInterface *resourceInterface = nullptr,
            BML_Mod owner = nullptr) {
            BML_ASSERT(resourceInterface);
            BML_ASSERT(owner);

            Handle h;
            h.m_ResourceInterface = resourceInterface;
            const auto result = resourceInterface->HandleCreate(owner, type, &h.m_desc);
            if (result == BML_RESULT_OK) {
                h.m_valid = true;
                return h;
            }
            return std::nullopt;
        }

        /**
         * @brief Wrap an existing handle descriptor
         * @param desc Handle descriptor
         * @param owns Whether to take ownership (release on destruction)
         */
        explicit Handle(const BML_HandleDesc &desc,
                        bool owns = false,
                        const BML_CoreResourceInterface *resourceInterface = nullptr)
            : m_desc(desc), m_valid(true), m_owns(owns), m_ResourceInterface(resourceInterface) {}

        ~Handle() {
            if (m_valid && m_owns && m_ResourceInterface && m_ResourceInterface->HandleRelease) {
                m_ResourceInterface->HandleRelease(&m_desc);
            }
        }

        // Non-copyable by default (use Retain() for reference counting)
        Handle(const Handle &) = delete;
        Handle &operator=(const Handle &) = delete;

        // Movable
        Handle(Handle &&other) noexcept
            : m_desc(other.m_desc),
              m_valid(other.m_valid),
              m_owns(other.m_owns),
              m_ResourceInterface(other.m_ResourceInterface) {
            other.m_valid = false;
            other.m_owns = false;
            other.m_ResourceInterface = nullptr;
        }

        Handle &operator=(Handle &&other) noexcept {
            if (this != &other) {
                if (m_valid && m_owns && m_ResourceInterface && m_ResourceInterface->HandleRelease) {
                    m_ResourceInterface->HandleRelease(&m_desc);
                }
                m_desc = other.m_desc;
                m_valid = other.m_valid;
                m_owns = other.m_owns;
                m_ResourceInterface = other.m_ResourceInterface;
                other.m_valid = false;
                other.m_owns = false;
                other.m_ResourceInterface = nullptr;
            }
            return *this;
        }

        /**
         * @brief Increment reference count
         * @return true if successful
         */
        bool Retain() {
            if (!m_valid) return false;
            return m_ResourceInterface->HandleRetain(&m_desc) == BML_RESULT_OK;
        }

        /**
         * @brief Decrement reference count
         * @return true if successful
         */
        bool Release() {
            if (!m_valid) return false;
            auto result = m_ResourceInterface->HandleRelease(&m_desc);
            if (result == BML_RESULT_OK) {
                m_valid = false;
            }
            return result == BML_RESULT_OK;
        }

        /**
         * @brief Check if handle is still valid
         * @return true if handle is valid
         */
        bool Validate() const {
            if (!m_valid) return false;
            BML_Bool valid = BML_FALSE;
            if (m_ResourceInterface->HandleValidate(&m_desc, &valid) == BML_RESULT_OK) {
                return valid != BML_FALSE;
            }
            return false;
        }

        /**
         * @brief Attach user data to the handle
         * @param data User data pointer
         * @return true if successful
         */
        bool AttachUserData(void *data) {
            if (!m_valid) return false;
            return m_ResourceInterface->HandleAttachUserData(&m_desc, data) == BML_RESULT_OK;
        }

        /**
         * @brief Get attached user data
         * @return User data pointer, or nullptr if not set or error
         */
        void *GetUserData() const {
            if (!m_valid) return nullptr;
            void *data = nullptr;
            if (m_ResourceInterface->HandleGetUserData(&m_desc, &data) == BML_RESULT_OK) {
                return data;
            }
            return nullptr;
        }

        /**
         * @brief Get attached user data (typed)
         * @tparam T Data type
         * @return Typed pointer
         */
        template <typename T>
        T *GetUserData() const {
            return static_cast<T *>(GetUserData());
        }

        /**
         * @brief Get handle type
         */
        BML_HandleType Type() const noexcept { return m_desc.type; }

        /**
         * @brief Get handle generation
         */
        uint32_t Generation() const noexcept { return m_desc.generation; }

        /**
         * @brief Get handle slot
         */
        uint32_t Slot() const noexcept { return m_desc.slot; }

        /**
         * @brief Get the raw descriptor
         */
        const BML_HandleDesc &Descriptor() const noexcept { return m_desc; }

        /**
         * @brief Check if handle is valid
         */
        explicit operator bool() const noexcept { return m_valid; }

    private:
        BML_HandleDesc m_desc{};
        bool m_valid = false;
        bool m_owns = true;
        const BML_CoreResourceInterface *m_ResourceInterface = nullptr;
    };

    // ============================================================================
    // Shared Handle (Reference Counted)
    // ============================================================================

    /**
     * @brief Reference-counted handle wrapper
     *
     * Unlike Handle, SharedHandle automatically manages reference counts
     * and can be copied. Each copy increments the reference count.
     *
     * Example:
     *   auto handle = bml::SharedHandle::create(MY_TYPE);
     *   auto copy = handle;  // Reference count incremented
     *   // Both go out of scope: only the last one releases
     */
    class SharedHandle {
    public:
        SharedHandle() : m_impl(nullptr) {}

        /**
         * @brief Create a new shared handle
         * @param type Handle type identifier
         * @return SharedHandle instance
         * @throws bml::Exception if creation fails
         */
        static SharedHandle create(BML_HandleType type,
                                   const BML_CoreResourceInterface *resourceInterface = nullptr,
                                   BML_Mod owner = nullptr) {
            BML_ASSERT(resourceInterface);
            BML_ASSERT(owner);

            auto impl = std::make_shared<Impl>();
            impl->resource_interface = resourceInterface;
            const auto result = resourceInterface->HandleCreate(owner, type, &impl->desc);
            if (result != BML_RESULT_OK) {
                throw Exception(result, "Failed to create handle");
            }
            impl->valid = true;

            SharedHandle h;
            h.m_impl = std::move(impl);
            return h;
        }

        /**
         * @brief Create without throwing
         * @param type Handle type identifier
         * @return SharedHandle if successful
         */
        static std::optional<SharedHandle> tryCreate(
            BML_HandleType type,
            const BML_CoreResourceInterface *resourceInterface = nullptr,
            BML_Mod owner = nullptr) {
            BML_ASSERT(resourceInterface);
            BML_ASSERT(owner);

            auto impl = std::make_shared<Impl>();
            impl->resource_interface = resourceInterface;
            const auto result = resourceInterface->HandleCreate(owner, type, &impl->desc);
            if (result == BML_RESULT_OK) {
                impl->valid = true;
                SharedHandle h;
                h.m_impl = std::move(impl);
                return h;
            }
            return std::nullopt;
        }

        // Copyable (shares reference)
        SharedHandle(const SharedHandle &) = default;
        SharedHandle &operator=(const SharedHandle &) = default;

        // Movable
        SharedHandle(SharedHandle &&) = default;
        SharedHandle &operator=(SharedHandle &&) = default;

        bool Validate() const {
            if (!m_impl || !m_impl->valid) {
                return false;
            }
            BML_Bool valid = BML_FALSE;
            if (m_impl->resource_interface->HandleValidate(&m_impl->desc, &valid) == BML_RESULT_OK) {
                return valid != BML_FALSE;
            }
            return false;
        }

        bool AttachUserData(void *data) {
            if (!m_impl || !m_impl->valid) {
                return false;
            }
            return m_impl->resource_interface->HandleAttachUserData(&m_impl->desc, data) == BML_RESULT_OK;
        }

        void *GetUserData() const {
            if (!m_impl || !m_impl->valid) {
                return nullptr;
            }
            void *data = nullptr;
            if (m_impl->resource_interface->HandleGetUserData(&m_impl->desc, &data) == BML_RESULT_OK) {
                return data;
            }
            return nullptr;
        }

        template <typename T>
        T *GetUserData() const {
            return static_cast<T *>(GetUserData());
        }

        BML_HandleType Type() const noexcept {
            return m_impl ? m_impl->desc.type : 0;
        }

        const BML_HandleDesc *Descriptor() const noexcept {
            return m_impl ? &m_impl->desc : nullptr;
        }

        explicit operator bool() const noexcept {
            return m_impl && m_impl->valid;
        }

        /**
         * @brief Get the current reference count
         */
        long UseCount() const noexcept {
            return m_impl.use_count();
        }

    private:
        struct Impl {
            BML_HandleDesc desc = BML_HANDLE_DESC_INIT;
            bool valid = false;
            const BML_CoreResourceInterface *resource_interface = nullptr;

            ~Impl() {
                if (valid && resource_interface && resource_interface->HandleRelease) {
                    resource_interface->HandleRelease(&desc);
                }
            }
        };

        std::shared_ptr<Impl> m_impl;
    };

} // namespace bml

#endif /* BML_RESOURCE_HPP */
