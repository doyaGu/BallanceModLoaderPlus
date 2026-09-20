// Variables and aliases of the console shell. Session variables live until the
// game exits; universal variables and every alias persist in one JSON file
// under the loader directory. Lookups see the session scope first.
#ifndef BML_SHELL_ENVIRONMENT_H
#define BML_SHELL_ENVIRONMENT_H

#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Console/Shell/ShellExpander.h"
#include "Console/Shell/ShellParser.h"

namespace BML::Shell {
    class Environment final : public VariableResolver, public AliasResolver {
    public:
        enum class Scope {
            Session,
            Universal,
        };

        using Entries = std::vector<std::pair<std::string, std::string>>;

        bool LookupVariable(std::string_view name, std::string &value) const override;
        bool LookupAlias(std::string_view name, std::string &body) const override;

        // A valid name starts with a letter or underscore and continues with
        // letters, digits, or underscores.
        static bool IsValidName(std::string_view name);

        void SetVariable(std::string_view name, std::string_view value, Scope scope);
        bool EraseVariable(std::string_view name); // both scopes
        bool HasVariable(std::string_view name) const;
        Entries Variables(Scope scope) const;
        std::vector<std::string> VariableNames() const; // both scopes, sorted, unique

        void SetAlias(std::string_view name, std::string_view body);
        bool RemoveAlias(std::string_view name);
        void ClearAliases();
        bool HasAlias(std::string_view name) const override;
        Entries Aliases() const;
        std::vector<std::string> AliasNames() const;

        // Universal variables and aliases as a JSON document, and back.
        std::string ToJson(std::string &error) const;
        bool FromJson(std::string_view json, std::string &error);

        // File forms of the above. A missing file loads as empty and succeeds.
        // Save writes through a temporary file and renames it into place.
        bool Load(const std::wstring &path, std::string &error);
        bool Save(const std::wstring &path, std::string &error) const;

        // Set whenever persisted state changed since the last Save or Load.
        bool IsDirty() const { return m_Dirty; }
        void ClearDirty() { m_Dirty = false; }

    private:
        std::map<std::string, std::string, std::less<>> m_Session;
        std::map<std::string, std::string, std::less<>> m_Universal;
        std::map<std::string, std::string, std::less<>> m_Aliases;
        bool m_Dirty = false;
    };
}

#endif // BML_SHELL_ENVIRONMENT_H
