#include "LocaleManager.h"

#include "KernelServices.h"

#include <algorithm>
#include <filesystem>

#include "Logging.h"
#include "StringUtils.h"

namespace BML::Core {
    namespace {
        constexpr char kLogCategory[] = "locale";
    }

    BML_Result LocaleManager::Load(const std::string &module_id,
                                   const std::wstring &mod_directory,
                                   const std::string &locale_code) {
        if (module_id.empty() || mod_directory.empty())
            return BML_RESULT_INVALID_ARGUMENT;

        // Remember directory for ReloadAll
        {
            std::unique_lock lock(m_Mutex);
            m_ModuleDirectories[module_id] = mod_directory;
        }

        std::filesystem::path base(mod_directory);
        std::filesystem::path locale_dir = base / L"locale";

        // Try the requested locale first
        std::filesystem::path locale_file = locale_dir / (locale_code + ".toml");

        StringTable table;
        BML_Result result = LoadFromFile(locale_file.wstring(), table);

        // Fall back to "en" if the requested locale was not found
        if (result == BML_RESULT_NOT_FOUND && locale_code != "en") {
            locale_file = locale_dir / "en.toml";
            result = LoadFromFile(locale_file.wstring(), table);
        }

        if (result != BML_RESULT_OK)
            return result;

        CoreLog(BML_LOG_DEBUG, kLogCategory,
                "Loaded locale '%s' for module '%s' (%zu strings)",
                locale_code.c_str(), module_id.c_str(), table.size());

        // Store under exclusive lock
        {
            std::unique_lock lock(m_Mutex);
            m_Tables[module_id] = std::move(table);
        }

        return BML_RESULT_OK;
    }

    const char *LocaleManager::Get(const std::string &module_id, const char *key) {
        if (!key)
            return nullptr;

        std::shared_lock lock(m_Mutex);

        auto mod_it = m_Tables.find(module_id);
        if (mod_it == m_Tables.end())
            return key;

        auto key_it = mod_it->second.find(key);
        if (key_it == mod_it->second.end())
            return key;

        return key_it->second.c_str();
    }

    BML_Result LocaleManager::SetLanguage(const char *language_code) {
        if (!language_code)
            return BML_RESULT_INVALID_ARGUMENT;

        std::unique_lock lock(m_Mutex);
        m_Language = language_code;
        return BML_RESULT_OK;
    }

    BML_Result LocaleManager::GetLanguage(const char **out_code) {
        if (!out_code)
            return BML_RESULT_INVALID_ARGUMENT;

        std::shared_lock lock(m_Mutex);
        *out_code = m_Language.c_str();
        return BML_RESULT_OK;
    }

    void LocaleManager::RegisterModuleDirectory(const std::string &module_id,
                                                const std::wstring &mod_directory) {
        std::unique_lock lock(m_Mutex);
        m_ModuleDirectories[module_id] = mod_directory;
    }

    void LocaleManager::ReloadAll() {
        // Copy current language and directory map under lock
        std::string language;
        std::unordered_map<std::string, std::wstring> dirs;
        {
            std::shared_lock lock(m_Mutex);
            language = m_Language;
            dirs = m_ModuleDirectories;
        }

        for (const auto &[module_id, directory] : dirs) {
            Load(module_id, directory, language);
        }

        CoreLog(BML_LOG_INFO, kLogCategory,
                "Reloaded locales for %zu modules (language=%s)",
                dirs.size(), language.c_str());
    }

    std::vector<std::string> LocaleManager::GetAvailableLocales(const std::wstring &mod_directory) {
        std::vector<std::string> result;

        std::filesystem::path locale_dir(mod_directory);
        locale_dir /= L"locale";

        std::error_code ec;
        if (!std::filesystem::is_directory(locale_dir, ec))
            return result;

        for (const auto &entry : std::filesystem::directory_iterator(locale_dir, ec)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().wstring();
            if (ext == L".toml") {
                result.push_back(utils::Utf16ToUtf8(entry.path().stem().wstring()));
            }
        }

        std::sort(result.begin(), result.end());
        return result;
    }

    const void *LocaleManager::BindTable(const std::string &module_id) {
        std::shared_lock lock(m_Mutex);
        auto it = m_Tables.find(module_id);
        if (it == m_Tables.end())
            return nullptr;
        return &it->second;
    }

    const char *LocaleManager::Lookup(const void *table, const char *key) {
        if (!table || !key)
            return key;
        auto *tbl = static_cast<const StringTable *>(table);
        auto it = tbl->find(std::string_view(key));
        if (it == tbl->end())
            return key;
        return it->second.c_str();
    }

    void LocaleManager::CleanupModule(const std::string &module_id) {
        if (module_id.empty())
            return;

        std::unique_lock lock(m_Mutex);
        m_Tables.erase(module_id);
        m_ModuleDirectories.erase(module_id);
    }

    void LocaleManager::Shutdown() {
        std::unique_lock lock(m_Mutex);
        m_Tables.clear();
        m_ModuleDirectories.clear();
        m_Language = "en";
    }

    void LocaleManager::FlattenTable(const toml::table &tbl,
                                     const std::string &prefix,
                                     StringTable &out) {
        for (auto &&[k, v] : tbl) {
            std::string full_key = prefix.empty()
                ? std::string(k.str())
                : prefix + "." + std::string(k.str());

            if (v.is_string()) {
                out.emplace(full_key, std::string(v.as_string()->get()));
            } else if (v.is_table()) {
                FlattenTable(*v.as_table(), full_key, out);
            }
            // Skip non-string, non-table values
        }
    }

    BML_Result LocaleManager::LoadFromFile(const std::wstring &path,
                                           StringTable &out_table) {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec))
            return BML_RESULT_NOT_FOUND;

        try {
            std::string path_utf8 = utils::Utf16ToUtf8(path);
            auto tbl = toml::parse_file(path_utf8);

            out_table.clear();
            FlattenTable(tbl, "", out_table);

            return BML_RESULT_OK;
        } catch (const toml::parse_error &e) {
            CoreLog(BML_LOG_WARN, kLogCategory,
                    "Failed to parse locale file %s: %s",
                    utils::Utf16ToUtf8(path).c_str(), e.what());
            return BML_RESULT_FAIL;
        } catch (...) {
            return BML_RESULT_INTERNAL_ERROR;
        }
    }

} // namespace BML::Core
