#ifndef BML_CONSOLE_BUILTIN_COMMANDS_H
#define BML_CONSOLE_BUILTIN_COMMANDS_H

#include "Console/CommandRecord.h"

class Console;

class CommandHelp {
public:
    CommandHelp() = default;

    std::string GetName() { return "help"; }

    std::string GetAlias() { return "?"; }

    std::string GetDescription() { return "Show Help Information about Existing Commands."; }

    bool IsCheat() { return false; }

    void Execute(ModContext *bml, const std::vector<std::string> &args);

    const std::vector<std::string> GetTabCompletion(ModContext *bml,
                                                    const std::vector<std::string> &args) {
        return {};
    }
};

class CommandEcho {
public:
    CommandEcho() = default;

    std::string GetName() { return "echo"; }

    std::string GetAlias() { return ""; }

    std::string GetDescription() { return "Output a line of string."; }

    bool IsCheat() { return false; }

    void Execute(ModContext *bml, const std::vector<std::string> &args);

    const std::vector<std::string> GetTabCompletion(ModContext *bml,
                                                    const std::vector<std::string> &args) {
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

class CommandClear {
public:
    explicit CommandClear(Console *console) : m_Console(console) {}

    std::string GetName() { return "clear"; }

    std::string GetAlias() { return ""; }

    std::string GetDescription() { return "Clear the Console."; }

    bool IsCheat() { return false; }

    void Execute(ModContext *bml, const std::vector<std::string> &args);

    const std::vector<std::string> GetTabCompletion(ModContext *bml,
                                                    const std::vector<std::string> &args) {
        return {};
    }

private:
    Console *m_Console;
};

class CommandHistory {
public:
    explicit CommandHistory(Console *console) : m_Console(console) {}

    std::string GetName() { return "history"; }

    std::string GetAlias() { return ""; }

    std::string GetDescription() {
        return "Show or edit the command history; rerun entries with !n.";
    }

    bool IsCheat() { return false; }

    void Execute(ModContext *bml, const std::vector<std::string> &args);

    const std::vector<std::string> GetTabCompletion(ModContext *bml,
                                                    const std::vector<std::string> &args) {
        if (args.size() == 2)
            return {"clear", "-c", "-d"};
        return {};
    }

private:
    Console *m_Console;
};

class CommandExit {
public:
    CommandExit() = default;

    std::string GetName() { return "exit"; }

    std::string GetAlias() { return ""; }

    std::string GetDescription() { return "Exit the game."; }

    bool IsCheat() { return false; }

    void Execute(ModContext *bml, const std::vector<std::string> &args);

    const std::vector<std::string> GetTabCompletion(ModContext *bml,
                                                    const std::vector<std::string> &args) {
        return {};
    }
};

#endif // BML_CONSOLE_BUILTIN_COMMANDS_H
