#ifndef BML_CONSOLE_BUILTIN_COMMANDS_H
#define BML_CONSOLE_BUILTIN_COMMANDS_H

#include "BML/ICommand.h"

class Console;

class CommandHelp : public ICommand {
public:
    CommandHelp() = default;

    std::string GetName() override { return "help"; }
    std::string GetAlias() override { return "?"; }
    std::string GetDescription() override { return "Show Help Information about Existing Commands."; }
    bool IsCheat() override { return false; }
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override { return {}; }
};

class CommandEcho : public ICommand {
public:
    CommandEcho() = default;

    std::string GetName() override { return "echo"; }
    std::string GetAlias() override { return ""; }
    std::string GetDescription() override { return "Output a line of string."; }
    bool IsCheat() override { return false; }
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override {
        if (args.size() == 2)
            return {"-e", "-n"};
        return {};
    }

    struct EchoOpts {
        bool interpretEscapes = false; // -e
        bool noNewline        = false; // -n
        bool parsingOptions   = true;  // stop at first non-option or "--"
    };

    static void ParseEchoOptionToken(const std::string &tok, EchoOpts &opt);
    static bool ApplyBackslashCTrunc(std::string &s);
};

class CommandClear : public ICommand {
public:
    explicit CommandClear(Console *console) : m_Console(console) {}

    std::string GetName() override { return "clear"; }
    std::string GetAlias() override { return ""; }
    std::string GetDescription() override { return "Clear the Console."; }
    bool IsCheat() override { return false; }
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override { return {}; }

private:
    Console *m_Console;
};

class CommandHistory : public ICommand {
public:
    explicit CommandHistory(Console *console) : m_Console(console) {}

    std::string GetName() override { return "history"; }
    std::string GetAlias() override { return ""; }
    std::string GetDescription() override { return "Show or edit the command history; rerun entries with !n."; }
    bool IsCheat() override { return false; }
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override {
        if (args.size() == 2)
            return {"clear", "-c", "-d"};
        return {};
    }

private:
    Console *m_Console;
};

class CommandExit : public ICommand {
public:
    CommandExit() = default;

    std::string GetName() override { return "exit"; }
    std::string GetAlias() override { return ""; }
    std::string GetDescription() override { return "Exit the game."; }
    bool IsCheat() override { return false; }
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override { return {}; }
};

#endif // BML_CONSOLE_BUILTIN_COMMANDS_H
