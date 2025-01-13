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
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override { return {}; }
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
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override { return {}; }

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
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override { return {"title", "fps", "sr"}; }

private:
    BMLMod *m_BMLMod;
    int m_State;
};

#endif // BML_COMMANDS_H