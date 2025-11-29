/**
 * @file bml_context.hpp
 * @brief BML C++ Context Wrapper
 * 
 * Provides RAII-friendly wrapper for BML_Context handles with optional
 * reference counting support.
 */

#ifndef BML_CONTEXT_HPP
#define BML_CONTEXT_HPP

#include "bml_core.h"
#include "bml_export.h"
#include "bml_loader.h"

namespace bml {
    // ============================================================================
    // API Loading (wraps bml_loader.h)
    // ============================================================================

    /**
     * @brief Load all BML API function pointers
     * @param get_proc Function to retrieve API pointers
     * @return true if successful
     */
    inline bool LoadAPI(PFN_BML_GetProcAddress get_proc) {
        return bmlLoadAPI(get_proc) == BML_RESULT_OK;
    }

    /**
     * @brief Unload all BML API function pointers
     */
    inline void UnloadAPI() {
        bmlUnloadAPI();
    }

    /**
     * @brief Check if BML API is loaded
     * @return true if API is loaded
     */
    inline bool IsApiLoaded() {
        return bmlIsApiLoaded() != 0;
    }

    // ============================================================================
    // Context Wrapper
    // ============================================================================

    /**
     * @brief Lightweight wrapper for BML_Context handle
     *
     * This is a non-owning handle wrapper. The context lifetime is managed
     * by the BML runtime, not by this wrapper.
     *
     * For RAII reference counting, use ScopedContext instead.
     */
    class Context {
    public:
        Context() : m_Ctx(nullptr) {}

        explicit Context(BML_Context ctx) : m_Ctx(ctx) {}

        /**
         * @brief Get the underlying handle
         */
        BML_Context Handle() const noexcept { return m_Ctx; }

        /**
         * @brief Check if context is valid
         */
        explicit operator bool() const noexcept { return m_Ctx != nullptr; }

        /**
         * @brief Retain (increment reference count)
         * @return true if successful
         */
        bool Retain() const {
            if (!m_Ctx || !bmlContextRetain) return false;
            return bmlContextRetain(m_Ctx) == BML_RESULT_OK;
        }

        /**
         * @brief Release (decrement reference count)
         * @return true if successful
         */
        bool Release() const {
            if (!m_Ctx || !bmlContextRelease) return false;
            return bmlContextRelease(m_Ctx) == BML_RESULT_OK;
        }

    private:
        BML_Context m_Ctx;
    };

    // ============================================================================
    // Scoped Context (RAII Reference Counting)
    // ============================================================================

    /**
     * @brief RAII wrapper for BML_Context with automatic reference counting
     *
     * Automatically retains the context on construction and releases on destruction.
     *
     * Example:
     *   {
     *       bml::ScopedContext ctx(bmlGetGlobalContext());
     *       // Use ctx...
     *   } // Automatically releases reference
     */
    class ScopedContext {
    public:
        ScopedContext() : m_Ctx(nullptr) {}

        /**
         * @brief Construct and retain context
         * @param ctx Context to wrap (will be retained)
         */
        explicit ScopedContext(BML_Context ctx) : m_Ctx(ctx) {
            if (m_Ctx && bmlContextRetain) {
                bmlContextRetain(m_Ctx);
            }
        }

        ~ScopedContext() {
            if (m_Ctx && bmlContextRelease) {
                bmlContextRelease(m_Ctx);
            }
        }

        // Non-copyable by default
        ScopedContext(const ScopedContext &) = delete;
        ScopedContext &operator=(const ScopedContext &) = delete;

        // Movable
        ScopedContext(ScopedContext &&other) noexcept : m_Ctx(other.m_Ctx) {
            other.m_Ctx = nullptr;
        }

        ScopedContext &operator=(ScopedContext &&other) noexcept {
            if (this != &other) {
                if (m_Ctx && bmlContextRelease) {
                    bmlContextRelease(m_Ctx);
                }
                m_Ctx = other.m_Ctx;
                other.m_Ctx = nullptr;
            }
            return *this;
        }

        BML_Context Handle() const noexcept { return m_Ctx; }
        Context GetContext() const noexcept { return Context(m_Ctx); }
        explicit operator bool() const noexcept { return m_Ctx != nullptr; }

    private:
        BML_Context m_Ctx;
    };

    // ============================================================================
    // Convenience Functions
    // ============================================================================

    /**
     * @brief Get the global BML context
     */
    inline Context GetGlobalContext() {
        return Context(bmlGetGlobalContext ? bmlGetGlobalContext() : nullptr);
    }
} // namespace bml

#endif /* BML_CONTEXT_HPP */
