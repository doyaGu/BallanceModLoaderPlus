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
     * @brief Load the host bootstrap minimum used to bind runtime interfaces later
     * @param get_proc Function to retrieve host bootstrap exports by name
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
        Context() : m_Ctx(nullptr), m_ContextInterface(nullptr) {}

        explicit Context(BML_Context ctx, const BML_CoreContextInterface *contextInterface = nullptr)
            : m_Ctx(ctx), m_ContextInterface(contextInterface) {}

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
            if (!m_Ctx || !m_ContextInterface || !m_ContextInterface->Retain) return false;
            return m_ContextInterface->Retain(m_Ctx) == BML_RESULT_OK;
        }

        /**
         * @brief Release (decrement reference count)
         * @return true if successful
         */
        bool Release() const {
            if (!m_Ctx || !m_ContextInterface || !m_ContextInterface->Release) return false;
            return m_ContextInterface->Release(m_Ctx) == BML_RESULT_OK;
        }

    private:
        BML_Context m_Ctx;
        const BML_CoreContextInterface *m_ContextInterface;
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
     *       bml::ScopedContext ctx(contextApi->Context, contextApi);
     *       // Use ctx...
     *   } // Automatically releases reference
     */
    class ScopedContext {
    public:
        ScopedContext() : m_Ctx(nullptr), m_ContextInterface(nullptr) {}

        /**
         * @brief Construct and retain context
         * @param ctx Context to wrap (will be retained)
         */
        explicit ScopedContext(BML_Context ctx,
                               const BML_CoreContextInterface *contextInterface = nullptr)
            : m_Ctx(ctx), m_ContextInterface(contextInterface) {
            if (m_Ctx && m_ContextInterface && m_ContextInterface->Retain) {
                m_ContextInterface->Retain(m_Ctx);
            }
        }

        ~ScopedContext() {
            if (m_Ctx && m_ContextInterface && m_ContextInterface->Release) {
                m_ContextInterface->Release(m_Ctx);
            }
        }

        // Non-copyable by default
        ScopedContext(const ScopedContext &) = delete;
        ScopedContext &operator=(const ScopedContext &) = delete;

        // Movable
        ScopedContext(ScopedContext &&other) noexcept
            : m_Ctx(other.m_Ctx), m_ContextInterface(other.m_ContextInterface) {
            other.m_Ctx = nullptr;
            other.m_ContextInterface = nullptr;
        }

        ScopedContext &operator=(ScopedContext &&other) noexcept {
            if (this != &other) {
                if (m_Ctx && m_ContextInterface && m_ContextInterface->Release) {
                    m_ContextInterface->Release(m_Ctx);
                }
                m_Ctx = other.m_Ctx;
                m_ContextInterface = other.m_ContextInterface;
                other.m_Ctx = nullptr;
                other.m_ContextInterface = nullptr;
            }
            return *this;
        }

        BML_Context Handle() const noexcept { return m_Ctx; }
        Context GetContext() const noexcept { return Context(m_Ctx, m_ContextInterface); }
        explicit operator bool() const noexcept { return m_Ctx != nullptr; }

    private:
        BML_Context m_Ctx;
        const BML_CoreContextInterface *m_ContextInterface;
    };

    // ============================================================================
    // Convenience Functions
    // ============================================================================

    /**
     * @brief Get the runtime-bound BML context through the acquired runtime interface
     */
    inline Context GetContext(const BML_CoreContextInterface *contextInterface = nullptr) {
        if (!contextInterface || !contextInterface->Context) {
            return Context();
        }
        return Context(contextInterface->Context, contextInterface);
    }
} // namespace bml

#endif /* BML_CONTEXT_HPP */
