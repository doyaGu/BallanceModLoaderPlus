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

#include <optional>
#include <memory>
#include <utility>

namespace bml {
    // ============================================================================
    // Resource Capabilities Query
    // ============================================================================

    /**
     * @brief Query resource subsystem capabilities
     * @return Capabilities if successful
     */
    inline std::optional<BML_ResourceCaps> GetResourceCaps() {
        if (!bmlResourceGetCaps) return std::nullopt;
        BML_ResourceCaps caps = BML_RESOURCE_CAPS_INIT;
        if (bmlResourceGetCaps(&caps) == BML_RESULT_OK) {
            return caps;
        }
        return std::nullopt;
    }

    /**
     * @brief Check if a resource capability is available
     * @param flag Capability flag to check
     * @return true if capability is available
     */
    inline bool HasResourceCap(BML_ResourceCapabilityFlags flag) {
        auto caps = GetResourceCaps();
        return caps && (caps->capability_flags & flag);
    }

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
     *   handle.attachUserData(my_data);
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
        static Handle create(BML_HandleType type) {
            if (!bmlHandleCreate) {
                throw Exception(BML_RESULT_NOT_FOUND, "Handle API unavailable");
            }

            Handle h;
            auto result = bmlHandleCreate(type, &h.m_desc);
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
        static std::optional<Handle> tryCreate(BML_HandleType type) {
            if (!bmlHandleCreate) return std::nullopt;

            Handle h;
            if (bmlHandleCreate(type, &h.m_desc) == BML_RESULT_OK) {
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
        explicit Handle(const BML_HandleDesc &desc, bool owns = false)
            : m_desc(desc), m_valid(true), m_owns(owns) {}

        ~Handle() {
            if (m_valid && m_owns && bmlHandleRelease) {
                bmlHandleRelease(&m_desc);
            }
        }

        // Non-copyable by default (use retain() for reference counting)
        Handle(const Handle &) = delete;
        Handle &operator=(const Handle &) = delete;

        // Movable
        Handle(Handle &&other) noexcept
            : m_desc(other.m_desc), m_valid(other.m_valid), m_owns(other.m_owns) {
            other.m_valid = false;
            other.m_owns = false;
        }

        Handle &operator=(Handle &&other) noexcept {
            if (this != &other) {
                if (m_valid && m_owns && bmlHandleRelease) {
                    bmlHandleRelease(&m_desc);
                }
                m_desc = other.m_desc;
                m_valid = other.m_valid;
                m_owns = other.m_owns;
                other.m_valid = false;
                other.m_owns = false;
            }
            return *this;
        }

        /**
         * @brief Increment reference count
         * @return true if successful
         */
        bool retain() {
            if (!m_valid || !bmlHandleRetain) return false;
            return bmlHandleRetain(&m_desc) == BML_RESULT_OK;
        }

        /**
         * @brief Decrement reference count
         * @return true if successful
         */
        bool release() {
            if (!m_valid || !bmlHandleRelease) return false;
            auto result = bmlHandleRelease(&m_desc);
            if (result == BML_RESULT_OK) {
                m_valid = false;
            }
            return result == BML_RESULT_OK;
        }

        /**
         * @brief Check if handle is still valid
         * @return true if handle is valid
         */
        bool validate() const {
            if (!m_valid || !bmlHandleValidate) return false;
            BML_Bool valid = BML_FALSE;
            if (bmlHandleValidate(&m_desc, &valid) == BML_RESULT_OK) {
                return valid != BML_FALSE;
            }
            return false;
        }

        /**
         * @brief Attach user data to the handle
         * @param data User data pointer
         * @return true if successful
         */
        bool attachUserData(void *data) {
            if (!m_valid || !bmlHandleAttachUserData) return false;
            return bmlHandleAttachUserData(&m_desc, data) == BML_RESULT_OK;
        }

        /**
         * @brief Get attached user data
         * @return User data pointer, or nullptr if not set or error
         */
        void *getUserData() const {
            if (!m_valid || !bmlHandleGetUserData) return nullptr;
            void *data = nullptr;
            if (bmlHandleGetUserData(&m_desc, &data) == BML_RESULT_OK) {
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
        T *getUserData() const {
            return static_cast<T *>(getUserData());
        }

        /**
         * @brief Get handle type
         */
        BML_HandleType type() const noexcept { return m_desc.type; }

        /**
         * @brief Get handle generation
         */
        uint32_t generation() const noexcept { return m_desc.generation; }

        /**
         * @brief Get handle slot
         */
        uint32_t slot() const noexcept { return m_desc.slot; }

        /**
         * @brief Get the raw descriptor
         */
        const BML_HandleDesc &descriptor() const noexcept { return m_desc; }

        /**
         * @brief Check if handle is valid
         */
        explicit operator bool() const noexcept { return m_valid; }

    private:
        BML_HandleDesc m_desc{};
        bool m_valid = false;
        bool m_owns = true;
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
        static SharedHandle create(BML_HandleType type) {
            if (!bmlHandleCreate) {
                throw Exception(BML_RESULT_NOT_FOUND, "Handle API unavailable");
            }

            auto impl = std::make_shared<Impl>();
            auto result = bmlHandleCreate(type, &impl->desc);
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
        static std::optional<SharedHandle> tryCreate(BML_HandleType type) {
            if (!bmlHandleCreate) return std::nullopt;

            auto impl = std::make_shared<Impl>();
            if (bmlHandleCreate(type, &impl->desc) == BML_RESULT_OK) {
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

        bool validate() const {
            if (!m_impl || !m_impl->valid || !bmlHandleValidate) return false;
            BML_Bool valid = BML_FALSE;
            if (bmlHandleValidate(&m_impl->desc, &valid) == BML_RESULT_OK) {
                return valid != BML_FALSE;
            }
            return false;
        }

        bool attachUserData(void *data) {
            if (!m_impl || !m_impl->valid || !bmlHandleAttachUserData) return false;
            return bmlHandleAttachUserData(&m_impl->desc, data) == BML_RESULT_OK;
        }

        void *getUserData() const {
            if (!m_impl || !m_impl->valid || !bmlHandleGetUserData) return nullptr;
            void *data = nullptr;
            if (bmlHandleGetUserData(&m_impl->desc, &data) == BML_RESULT_OK) {
                return data;
            }
            return nullptr;
        }

        template <typename T>
        T *getUserData() const {
            return static_cast<T *>(getUserData());
        }

        BML_HandleType type() const noexcept {
            return m_impl ? m_impl->desc.type : 0;
        }

        const BML_HandleDesc *descriptor() const noexcept {
            return m_impl ? &m_impl->desc : nullptr;
        }

        explicit operator bool() const noexcept {
            return m_impl && m_impl->valid;
        }

        /**
         * @brief Get the current reference count
         */
        long use_count() const noexcept {
            return m_impl.use_count();
        }

    private:
        struct Impl {
            BML_HandleDesc desc = BML_HANDLE_DESC_INIT;
            bool valid = false;

            ~Impl() {
                if (valid && bmlHandleRelease) {
                    bmlHandleRelease(&desc);
                }
            }
        };

        std::shared_ptr<Impl> m_impl;
    };
} // namespace bml

#endif /* BML_RESOURCE_HPP */
