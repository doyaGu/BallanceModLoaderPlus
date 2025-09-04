#ifndef BML_COMMANDS_H
#define BML_COMMANDS_H

#include "BML/ICommand.h"

class BMLMod;

class CommandBML : public ICommand {
public:
    std::string GetName() override { return "bml"; }
    std::string GetAlias() override { return ""; }
    std::string GetDescription() override { return "Show Information about Ballance Mod Loader."; }
    bool IsCheat() override { return false; }
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override { return {}; }
};

class CommandHelp : public ICommand {
public:
    std::string GetName() override { return "help"; }
    std::string GetAlias() override { return "?"; }
    std::string GetDescription() override { return "Show Help Information about Existing Commands."; }
    bool IsCheat() override { return false; }
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override { return {}; }
};

class CommandCheat : public ICommand {
public:
    std::string GetName() override { return "cheat"; }
    std::string GetAlias() override { return ""; }
    std::string GetDescription() override { return "Enable or Disable Cheat Mode."; }
    bool IsCheat() override { return false; }
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override {
        return args.size() == 2 ? std::vector<std::string>({"true", "false"}) : std::vector<std::string>();
    }
};

class CommandEcho : public ICommand {
public:
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
    static std::string JoinArgs(const std::vector<std::string> &args, size_t start);
    static bool ApplyBackslashCTrunc(std::string &s);
};

class CommandClear : public ICommand {
public:
    explicit CommandClear(BMLMod *mod) : m_BMLMod(mod) {}

    std::string GetName() override { return "clear"; }
    std::string GetAlias() override { return ""; }
    std::string GetDescription() override { return "Clear the Console."; }
    bool IsCheat() override { return false; }
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override { return {}; }

private:
    BMLMod *m_BMLMod;
};

class CommandHistory : public ICommand {
public:
    explicit CommandHistory(BMLMod *mod) : m_BMLMod(mod) {}

    std::string GetName() override { return "history"; }
    std::string GetAlias() override { return ""; }
    std::string GetDescription() override { return "Manage command history."; }
    bool IsCheat() override { return false; }
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override {
        if (args.size() == 2)
            return {"clear"};
        return {};
    }

private:
    BMLMod *m_BMLMod;
};

class CommandExit : public ICommand {
public:
    std::string GetName() override { return "exit"; }
    std::string GetAlias() override { return ""; }
    std::string GetDescription() override { return "Exit the game."; }
    bool IsCheat() override { return false; }
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override { return {}; }
};

class CommandHUD : public ICommand {
public:
    explicit CommandHUD(BMLMod *mod);

    std::string GetName() override { return "hud"; }
    std::string GetAlias() override { return ""; }
    std::string GetDescription() override { return "Commands for HUD."; }
    bool IsCheat() override { return false; }
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override {
        if (args.size() == 2)
            return {"title", "fps", "sr", "on", "off"};
        if (args.size() == 3)
            return {"on", "off"};
        return {};
    }

private:
    BMLMod *m_BMLMod;
    int m_State;
};

class CommandPalette : public ICommand {
public:
    explicit CommandPalette(BMLMod *mod) : m_BMLMod(mod) {}
    std::string GetName() override { return "palette"; }
    std::string GetAlias() override { return "pal"; }
    std::string GetDescription() override { return "Manage ANSI 256-color palette."; }
    bool IsCheat() override { return false; }
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override {
        if (args.size() == 2) return {"reload", "sample"};
        return {};
    }
private:
    BMLMod *m_BMLMod;
};

#endif // BML_COMMANDS_H
