/**
 * @file bml_locale.hpp
 * @brief C++ RAII wrapper for the BML localization API
 *
 * Provides `bml::Locale` with a fast-path `operator[]` that caches the
 * module's string table handle. Locale calls always use the bound module
 * handle explicitly.
 */

#ifndef BML_LOCALE_HPP
#define BML_LOCALE_HPP

#include "bml_core.hpp"

#include <cstddef>
#include <string_view>

namespace bml {
    namespace detail {
        template <typename MemberT>
        constexpr bool HasLocaleMember(const BML_CoreLocaleInterface *iface, size_t offset) noexcept {
            return iface != nullptr && iface->header.struct_size >= offset + sizeof(MemberT);
        }
    } // namespace detail

#define BML_CORE_LOCALE_HAS_MEMBER(iface, member) \
    (::bml::detail::HasLocaleMember<decltype(((BML_CoreLocaleInterface *) 0)->member)>( \
        (iface), offsetof(BML_CoreLocaleInterface, member)) && (iface)->member != nullptr)

    /**
     * @brief C++ wrapper for per-module locale string tables.
     *
     * Fast path: after Load(), operator[] uses a cached table pointer
     * for direct lookup.
     *
     * Usage:
     *     auto &loc = Services().Locale();
     *     loc.Load(nullptr);  // load current global language
     *     ImGui::Begin(loc["window.title"]);
     */
    class Locale {
    public:
        explicit Locale(const BML_CoreLocaleInterface *iface = nullptr) noexcept
            : m_Interface(iface) {}

        Locale(BML_Mod mod,
               const BML_CoreLocaleInterface *iface,
               const BML_CoreModuleInterface *moduleInterface) noexcept
            : m_Interface(iface),
              m_Module(mod, moduleInterface) {}

        /**
         * @brief Load a locale file for the bound module.
         * Invalidates the cached table and re-binds automatically on success.
         */
        bool Load(const char *locale_code = nullptr) const {
            if (!m_Interface)
                return false;

            if (!m_Module || !m_Interface->Load) {
                return false;
            }
            const bool ok = m_Interface->Load(m_Module.Handle(), locale_code) == BML_RESULT_OK;
            Invalidate();
            if (ok)
                Bind();
            return ok;
        }

        /**
         * @brief Get a localized string by key.
         *
         * Uses the cached table handle when available. If no table is bound yet,
         * tries to bind on demand before falling back to the slow path.
         */
        const char *Get(const char *key) const {
            if (!key)
                return nullptr;

            if (!m_CachedTable && m_Interface && BML_CORE_LOCALE_HAS_MEMBER(m_Interface, BindTable))
                Bind();

            if (m_CachedTable && m_Interface && m_Interface->Lookup)
                return m_Interface->Lookup(m_CachedTable, key);

            if (m_Interface && m_Module && m_Interface->Get) {
                return m_Interface->Get(m_Module.Handle(), key);
            }

            return nullptr;
        }

        /** @brief Shorthand for Get(). */
        const char *operator[](const char *key) const {
            return Get(key);
        }

        /**
         * @brief Change the global language and refresh the cached table.
         */
        bool SetLanguage(const char *language_code) const {
            if (!m_Interface || !m_Interface->SetLanguage)
                return false;

            const bool ok = m_Interface->SetLanguage(language_code) == BML_RESULT_OK;
            if (ok) {
                Invalidate();
                Bind();
            }
            return ok;
        }

        std::string_view GetLanguage() const {
            if (!m_Interface || !m_Interface->GetLanguage)
                return {};

            const char *code = nullptr;
            if (m_Interface->GetLanguage(&code) != BML_RESULT_OK || !code)
                return {};
            return code;
        }

        explicit operator bool() const noexcept {
            return m_Interface != nullptr;
        }

    private:
        const BML_CoreLocaleInterface *m_Interface = nullptr;
        Mod m_Module;
        mutable BML_LocaleTable m_CachedTable = nullptr;

        void Invalidate() const {
            m_CachedTable = nullptr;
        }

        void Bind() const {
            if (m_Interface && m_Module && BML_CORE_LOCALE_HAS_MEMBER(m_Interface, BindTable)) {
                m_Interface->BindTable(m_Module.Handle(), &m_CachedTable);
            }
        }
    };

#undef BML_CORE_LOCALE_HAS_MEMBER

} // namespace bml

#endif /* BML_LOCALE_HPP */
