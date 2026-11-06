// Commands the shell itself provides: set, alias, unalias, true, false, xargs.
// They are ordinary BML::CommandRecord objects registered by the console, so help lists
// them and Tab completes them like everything else.
#ifndef BML_SHELL_BUILTINS_H
#define BML_SHELL_BUILTINS_H

#include <functional>
#include <string>
#include <vector>

#include "Console/CommandRecord.h"

namespace BML::Shell {
    class Environment;

    // Runs one already-split command; what xargs calls for every batch.
    using InvokeFunction = std::function<int(const std::vector<std::string> &args, const std::string *input)>;

    class CommandSet final {
    public:
        explicit CommandSet(Environment &environment) : m_Environment(environment) {}

        std::string GetName() { return "set"; }

        std::string GetAlias() { return ""; }

        std::string GetDescription() { return "List, set, or erase shell variables ($NAME)."; }

        bool IsCheat() { return false; }

        void Execute(ModContext *bml, const std::vector<std::string> &args);
        const std::vector<std::string> GetTabCompletion(ModContext *bml,
                                                        const std::vector<std::string> &args);

    private:
        Environment &m_Environment;
    };

    class CommandAlias final {
    public:
        explicit CommandAlias(Environment &environment) : m_Environment(environment) {}

        std::string GetName() { return "alias"; }

        std::string GetAlias() { return ""; }

        std::string GetDescription() { return "Define or list command aliases."; }

        bool IsCheat() { return false; }

        void Execute(ModContext *bml, const std::vector<std::string> &args);
        const std::vector<std::string> GetTabCompletion(ModContext *bml,
                                                        const std::vector<std::string> &args);

    private:
        Environment &m_Environment;
    };

    class CommandUnalias final {
    public:
        explicit CommandUnalias(Environment &environment) : m_Environment(environment) {}

        std::string GetName() { return "unalias"; }

        std::string GetAlias() { return ""; }

        std::string GetDescription() { return "Remove command aliases."; }

        bool IsCheat() { return false; }

        void Execute(ModContext *bml, const std::vector<std::string> &args);
        const std::vector<std::string> GetTabCompletion(ModContext *bml,
                                                        const std::vector<std::string> &args);

    private:
        Environment &m_Environment;
    };

    class CommandTrue final {
    public:
        std::string GetName() { return "true"; }

        std::string GetAlias() { return ""; }

        std::string GetDescription() { return "Do nothing, successfully."; }

        bool IsCheat() { return false; }

        void Execute(ModContext *bml, const std::vector<std::string> &args) {}

        const std::vector<std::string> GetTabCompletion(ModContext *,
                                                        const std::vector<std::string> &) {
            return {};
        }
    };

    class CommandFalse final {
    public:
        std::string GetName() { return "false"; }

        std::string GetAlias() { return ""; }

        std::string GetDescription() { return "Do nothing, unsuccessfully."; }

        bool IsCheat() { return false; }

        void Execute(ModContext *bml, const std::vector<std::string> &args);

        const std::vector<std::string> GetTabCompletion(ModContext *,
                                                        const std::vector<std::string> &) {
            return {};
        }
    };

    class CommandXargs final {
    public:
        explicit CommandXargs(InvokeFunction invoke) : m_Invoke(std::move(invoke)) {}

        std::string GetName() { return "xargs"; }

        std::string GetAlias() { return ""; }

        std::string GetDescription() { return "Run a command with piped text as its arguments."; }

        bool IsCheat() { return false; }

        void Execute(ModContext *bml, const std::vector<std::string> &args);
        const std::vector<std::string> GetTabCompletion(ModContext *bml,
                                                        const std::vector<std::string> &args);

        // Splits input into items: on delimiter when given, else on whitespace and
        // newlines with empty items dropped.
        static std::vector<std::string> SplitItems(const std::string &input, const std::string *delimiter);

    private:
        InvokeFunction m_Invoke;
    };
}

#endif // BML_SHELL_BUILTINS_H
