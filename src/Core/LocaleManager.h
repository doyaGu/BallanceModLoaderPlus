#ifndef BML_CORE_LOCALE_MANAGER_H
#define BML_CORE_LOCALE_MANAGER_H

#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <toml++/toml.hpp>

#include "bml_errors.h"

namespace BML::Core {

    class LocaleManager {
    public:
        static LocaleManager &Instance();

        /**
         * @brief Load a locale file for a module.
         *
         * Searches for `<mod_directory>/locale/<locale_code>.toml`.
         * Falls back to "en" if the requested code is not found.
         * Replaces any existing locale data for the module.
         *
         * @param module_id   Module identifier
         * @param mod_directory Module directory (wide string from manifest)
         * @param locale_code Requested locale code (e.g. "en", "zh-CN")
         * @return BML_RESULT_OK on success
         * @return BML_RESULT_NOT_FOUND if no locale file found
         */
        BML_Result Load(const std::string &module_id,
                        const std::wstring &mod_directory,
                        const std::string &locale_code);

        /**
         * @brief Get a localized string for a module.
         *
         * @param module_id Module identifier
         * @param key       String table key
         * @return Pointer to the localized string, or the key itself if not found.
         *         For the key-fallback case, the returned pointer is the input key.
         *         Otherwise, the pointer is valid until the module's locale data is
         *         reloaded or cleaned up.
         */
        const char *Get(const std::string &module_id, const char *key);

        /**
         * @brief Set the global language code.
         * @param language_code Language code (e.g. "en", "zh-CN")
         */
        BML_Result SetLanguage(const char *language_code);

        /**
         * @brief Get the current global language code.
         * @param out_code Receives pointer to the language code string
         */
        BML_Result GetLanguage(const char **out_code);

        /**
         * @brief Get an opaque pointer to a module's string table for fast lookup.
         * The pointer is invalidated when Load() is called for that module.
         */
        const void *BindTable(const std::string &module_id);

        /**
         * @brief Look up a key directly from a bound table (fast path).
         * No mutex, no module resolution. The table must be valid.
         */
        static const char *Lookup(const void *table, const char *key);

        /**
         * @brief Remove all locale data for a module.
         * Called during module detach/cleanup.
         */
        void CleanupModule(const std::string &module_id);

        /**
         * @brief Reload locale for all modules that previously loaded successfully.
         * Called after SetLanguage to apply the new language.
         */
        void ReloadAll();

        /**
         * @brief Get the list of available locale codes for a module directory.
         * Scans <mod_dir>/locale/ for *.toml files.
         */
        std::vector<std::string> GetAvailableLocales(const std::wstring &mod_directory);

        /**
         * @brief Register a module directory so ReloadAll can find it.
         * Called automatically by Load.
         */
        void RegisterModuleDirectory(const std::string &module_id,
                                     const std::wstring &mod_directory);

        /**
         * @brief Clear all state (called during shutdown).
         */
        void Shutdown();

        LocaleManager() = default;

    private:
        // Heterogeneous-lookup hash/equal for string_view key access without allocation
        struct StringHash {
            using is_transparent = void;
            size_t operator()(std::string_view sv) const noexcept {
                return std::hash<std::string_view>{}(sv);
            }
        };
        struct StringEqual {
            using is_transparent = void;
            bool operator()(std::string_view a, std::string_view b) const noexcept {
                return a == b;
            }
        };
        using StringTable = std::unordered_map<std::string, std::string, StringHash, StringEqual>;

        BML_Result LoadFromFile(const std::wstring &path, StringTable &out_table);
        void FlattenTable(const toml::table &tbl, const std::string &prefix,
                          StringTable &out);

        mutable std::shared_mutex m_Mutex;
        std::unordered_map<std::string, StringTable> m_Tables;
        // module_id -> directory, for ReloadAll
        std::unordered_map<std::string, std::wstring> m_ModuleDirectories;
        std::string m_Language{"en"};
    };

} // namespace BML::Core

#endif // BML_CORE_LOCALE_MANAGER_H
