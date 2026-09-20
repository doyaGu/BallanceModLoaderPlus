#include "Console/Shell/ShellEnvironment.h"

#include <cstdint>
#include <set>

#include "Console/Shell/ShellQuoting.h"
#include "JsonUtils.h"
#include "PathUtils.h"

namespace BML::Shell {
    namespace {
        constexpr int kFormatVersion = 1;

        bool ReadStringMap(yyjson_val *object, std::map<std::string, std::string, std::less<>> &out,
                           bool requireValidNames) {
            if (!object)
                return true;
            if (!yyjson_is_obj(object))
                return false;
            yyjson_obj_iter iter = yyjson_obj_iter_with(object);
            while (yyjson_val *key = yyjson_obj_iter_next(&iter)) {
                yyjson_val *value = yyjson_obj_iter_get_val(key);
                const char *name = yyjson_get_str(key);
                const char *text = yyjson_get_str(value);
                if (!name || !text)
                    continue;
                if (requireValidNames && !Environment::IsValidName(name))
                    continue;
                out[name] = text;
            }
            return true;
        }

        std::vector<std::string> SortedKeys(const std::map<std::string, std::string, std::less<>> &map) {
            std::vector<std::string> keys;
            keys.reserve(map.size());
            for (const auto &entry : map)
                keys.push_back(entry.first);
            return keys;
        }
    }

    bool Environment::IsValidName(std::string_view name) {
        if (name.empty() || !IsNameStart(name[0]))
            return false;
        for (char c : name) {
            if (!IsNameChar(c))
                return false;
        }
        return true;
    }

    bool Environment::LookupVariable(std::string_view name, std::string &value) const {
        if (auto it = m_Session.find(name); it != m_Session.end()) {
            value = it->second;
            return true;
        }
        if (auto it = m_Universal.find(name); it != m_Universal.end()) {
            value = it->second;
            return true;
        }
        return false;
    }

    bool Environment::LookupAlias(std::string_view name, std::string &body) const {
        const auto it = m_Aliases.find(name);
        if (it == m_Aliases.end())
            return false;
        body = it->second;
        return true;
    }

    void Environment::SetVariable(std::string_view name, std::string_view value, Scope scope) {
        if (scope == Scope::Universal) {
            m_Universal[std::string(name)] = std::string(value);
            m_Session.erase(std::string(name));
            m_Dirty = true;
        } else {
            m_Session[std::string(name)] = std::string(value);
        }
    }

    bool Environment::EraseVariable(std::string_view name) {
        const std::string key(name);
        const bool session = m_Session.erase(key) > 0;
        const bool universal = m_Universal.erase(key) > 0;
        if (universal)
            m_Dirty = true;
        return session || universal;
    }

    bool Environment::HasVariable(std::string_view name) const {
        return m_Session.find(name) != m_Session.end() || m_Universal.find(name) != m_Universal.end();
    }

    Environment::Entries Environment::Variables(Scope scope) const {
        const auto &map = scope == Scope::Universal ? m_Universal : m_Session;
        return Entries(map.begin(), map.end());
    }

    std::vector<std::string> Environment::VariableNames() const {
        std::set<std::string> names;
        for (const auto &entry : m_Session)
            names.insert(entry.first);
        for (const auto &entry : m_Universal)
            names.insert(entry.first);
        return std::vector<std::string>(names.begin(), names.end());
    }

    void Environment::SetAlias(std::string_view name, std::string_view body) {
        m_Aliases[std::string(name)] = std::string(body);
        m_Dirty = true;
    }

    bool Environment::RemoveAlias(std::string_view name) {
        if (m_Aliases.erase(std::string(name)) == 0)
            return false;
        m_Dirty = true;
        return true;
    }

    void Environment::ClearAliases() {
        if (m_Aliases.empty())
            return;
        m_Aliases.clear();
        m_Dirty = true;
    }

    bool Environment::HasAlias(std::string_view name) const {
        return m_Aliases.find(name) != m_Aliases.end();
    }

    Environment::Entries Environment::Aliases() const {
        return Entries(m_Aliases.begin(), m_Aliases.end());
    }

    std::vector<std::string> Environment::AliasNames() const {
        return SortedKeys(m_Aliases);
    }

    std::string Environment::ToJson(std::string &error) const {
        utils::MutableJsonDocument doc;
        if (!doc.IsValid()) {
            error = "failed to allocate a JSON document";
            return {};
        }
        yyjson_mut_val *root = doc.CreateObject();
        yyjson_mut_val *variables = doc.CreateObject();
        yyjson_mut_val *aliases = doc.CreateObject();
        if (!root || !variables || !aliases) {
            error = "failed to allocate a JSON object";
            return {};
        }
        doc.SetRoot(root);
        if (!doc.AddInt(root, "version", kFormatVersion) ||
            !doc.AddValue(root, "variables", variables) ||
            !doc.AddValue(root, "aliases", aliases)) {
            error = "failed to build the JSON document";
            return {};
        }
        for (const auto &entry : m_Universal) {
            if (!doc.AddString(variables, entry.first, entry.second)) {
                error = "failed to write variable " + entry.first;
                return {};
            }
        }
        for (const auto &entry : m_Aliases) {
            if (!doc.AddString(aliases, entry.first, entry.second)) {
                error = "failed to write alias " + entry.first;
                return {};
            }
        }
        return doc.Write(true, error);
    }

    bool Environment::FromJson(std::string_view json, std::string &error) {
        utils::JsonDocument doc = utils::JsonDocument::Parse(json, error);
        if (!doc.IsValid())
            return false;
        yyjson_val *root = doc.Root();
        if (!yyjson_is_obj(root)) {
            error = "the shell state file does not hold a JSON object";
            return false;
        }

        std::map<std::string, std::string, std::less<>> universal;
        std::map<std::string, std::string, std::less<>> aliases;
        if (!ReadStringMap(yyjson_obj_get(root, "variables"), universal, true)) {
            error = "\"variables\" is not a JSON object";
            return false;
        }
        if (!ReadStringMap(yyjson_obj_get(root, "aliases"), aliases, true)) {
            error = "\"aliases\" is not a JSON object";
            return false;
        }

        m_Universal = std::move(universal);
        m_Aliases = std::move(aliases);
        m_Dirty = false;
        return true;
    }

    bool Environment::Load(const std::wstring &path, std::string &error) {
        if (path.empty()) {
            error = "no path";
            return false;
        }
        if (!utils::FileExistsW(path)) {
            m_Universal.clear();
            m_Aliases.clear();
            m_Dirty = false;
            return true;
        }
        const std::vector<std::uint8_t> bytes = utils::ReadBinaryFileW(path);
        if (bytes.empty()) {
            m_Universal.clear();
            m_Aliases.clear();
            m_Dirty = false;
            return true;
        }
        return FromJson(std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size()), error);
    }

    bool Environment::Save(const std::wstring &path, std::string &error) const {
        if (path.empty()) {
            error = "no path";
            return false;
        }
        const std::string json = ToJson(error);
        if (json.empty())
            return false;

        const std::wstring tempPath = path + L".tmp";
        const std::vector<std::uint8_t> bytes(json.begin(), json.end());
        if (!utils::WriteBinaryFileW(tempPath, bytes)) {
            utils::DeleteFileW(tempPath);
            error = "failed to write the shell state file";
            return false;
        }
        if (!utils::MoveFileW(tempPath, path)) {
            utils::DeleteFileW(tempPath);
            error = "failed to replace the shell state file";
            return false;
        }
        return true;
    }
}
