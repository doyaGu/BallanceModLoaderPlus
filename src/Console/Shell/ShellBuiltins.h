// Commands the shell itself provides: set, alias, unalias, true, false, xargs.
// They are ordinary ICommand objects registered by the console, so help lists
// them and Tab completes them like everything else.
#ifndef BML_SHELL_BUILTINS_H
#define BML_SHELL_BUILTINS_H

#include <functional>
#include <string>
#include <vector>

#include "BML/ICommand.h"

namespace BML::Shell {
    class Environment;

    // Runs one already-split command; what xargs calls for every batch.
    using InvokeFunction = std::function<int(const std::vector<std::string> &args, const std::string *input)>;

    class CommandSet final : public ICommand {
    public:
        explicit CommandSet(Environment &environment) : m_Environment(environment) {}

        std::string GetName() override { return "set"; }
        std::string GetAlias() override { return ""; }
        std::string GetDescription() override { return "List, set, or erase shell variables ($NAME)."; }
        bool IsCheat() override { return false; }
        void Execute(IBML *bml, const std::vector<std::string> &args) override;
        const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override;

    private:
        Environment &m_Environment;
    };

    class CommandAlias final : public ICommand {
    public:
        explicit CommandAlias(Environment &environment) : m_Environment(environment) {}

        std::string GetName() override { return "alias"; }
        std::string GetAlias() override { return ""; }
        std::string GetDescription() override { return "Define or list command aliases."; }
        bool IsCheat() override { return false; }
        void Execute(IBML *bml, const std::vector<std::string> &args) override;
        const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override;

    private:
        Environment &m_Environment;
    };

    class CommandUnalias final : public ICommand {
    public:
        explicit CommandUnalias(Environment &environment) : m_Environment(environment) {}

        std::string GetName() override { return "unalias"; }
        std::string GetAlias() override { return ""; }
        std::string GetDescription() override { return "Remove command aliases."; }
        bool IsCheat() override { return false; }
        void Execute(IBML *bml, const std::vector<std::string> &args) override;
        const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override;

    private:
        Environment &m_Environment;
    };

    class CommandTrue final : public ICommand {
    public:
        std::string GetName() override { return "true"; }
        std::string GetAlias() override { return ""; }
        std::string GetDescription() override { return "Do nothing, successfully."; }
        bool IsCheat() override { return false; }
        void Execute(IBML *bml, const std::vector<std::string> &args) override {}
        const std::vector<std::string> GetTabCompletion(IBML *, const std::vector<std::string> &) override { return {}; }
    };

    class CommandFalse final : public ICommand {
    public:
        std::string GetName() override { return "false"; }
        std::string GetAlias() override { return ""; }
        std::string GetDescription() override { return "Do nothing, unsuccessfully."; }
        bool IsCheat() override { return false; }
        void Execute(IBML *bml, const std::vector<std::string> &args) override;
        const std::vector<std::string> GetTabCompletion(IBML *, const std::vector<std::string> &) override { return {}; }
    };

    class CommandXargs final : public ICommand {
    public:
        explicit CommandXargs(InvokeFunction invoke) : m_Invoke(std::move(invoke)) {}

        std::string GetName() override { return "xargs"; }
        std::string GetAlias() override { return ""; }
        std::string GetDescription() override { return "Run a command with piped text as its arguments."; }
        bool IsCheat() override { return false; }
        void Execute(IBML *bml, const std::vector<std::string> &args) override;
        const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override;

        // Splits input into items: on delimiter when given, else on whitespace and
        // newlines with empty items dropped.
        static std::vector<std::string> SplitItems(const std::string &input, const std::string *delimiter);

    private:
        InvokeFunction m_Invoke;
    };
}

#endif // BML_SHELL_BUILTINS_H
