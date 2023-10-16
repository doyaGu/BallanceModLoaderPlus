#ifndef BML_COMMANDS_H
#define BML_COMMANDS_H

#include <map>

#include "CKAll.h"

#include "ICommand.h"

class BMLMod;

class CommandBML : public ICommand {
public:
    std::string GetName() override { return "bml"; };
    std::string GetAlias() override { return ""; };
    std::string GetDescription() override { return "Show Information about Ballance Mod Loader."; };
    bool IsCheat() override { return false; };
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override { return {}; };
};

class CommandHelp : public ICommand {
public:
    std::string GetName() override { return "help"; };
    std::string GetAlias() override { return "?"; };
    std::string GetDescription() override { return "Show Help Information about Existing Commands."; };
    bool IsCheat() override { return false; };
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override { return {}; };
};

class CommandCheat : public ICommand {
public:
    std::string GetName() override { return "cheat"; };
    std::string GetAlias() override { return ""; };
    std::string GetDescription() override { return "Enable or Disable Cheat Mode."; };
    bool IsCheat() override { return false; };
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override {
        return args.size() == 2 ? std::vector<std::string>({"true", "false"}) : std::vector<std::string>();
    };
};

class CommandClear : public ICommand {
public:
    explicit CommandClear(BMLMod *mod) : m_BMLMod(mod) {};

    std::string GetName() override { return "clear"; };
    std::string GetAlias() override { return ""; };
    std::string GetDescription() override { return "Clear the Console."; };
    bool IsCheat() override { return false; };
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override { return {}; };

private:
    BMLMod *m_BMLMod;
};

class CommandScore : public ICommand {
public:
    std::string GetName() override { return "score"; };
    std::string GetAlias() override { return ""; };
    std::string GetDescription() override { return "Manage Ingame Score."; };
    bool IsCheat() override { return true; };
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override {
        return args.size() == 2 ? std::vector<std::string>({"add", "sub", "set"}) : std::vector<std::string>();
    };

private:
    CKDataArray *m_Energy = nullptr;
};

class CommandKill : public ICommand {
public:
    std::string GetName() override { return "kill"; };
    std::string GetAlias() override { return ""; };
    std::string GetDescription() override { return "Suicide."; };
    bool IsCheat() override { return false; };
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override { return {}; };

private:
    CKBehavior *m_DeactivateBall = nullptr;
};

class CommandSetSpawn : public ICommand {
public:
    std::string GetName() override { return "spawn"; };
    std::string GetAlias() override { return ""; };
    std::string GetDescription() override { return "Set Ball Spawn Point to Current Position."; };
    bool IsCheat() override { return true; };
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override { return {}; };

private:
    CKDataArray *m_CurLevel = nullptr;
};

class CommandWin : public ICommand {
public:
    std::string GetName() override { return "win"; };
    std::string GetAlias() override { return ""; };
    std::string GetDescription() override { return "Finish this Level."; };
    bool IsCheat() override { return true; };
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override { return {}; };
};

class CommandSector : public ICommand {
public:
    explicit CommandSector(BMLMod *mod) : m_BMLMod(mod) {};

    std::string GetName() override { return "sector"; };
    std::string GetAlias() override { return ""; };
    std::string GetDescription() override { return "Start playing specified sector."; };
    bool IsCheat() override { return true; };
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override { return {}; };

private:
    BMLMod *m_BMLMod;
};

class CommandSpeed : public ICommand {
public:
    explicit CommandSpeed(BMLMod *mod) : m_BMLMod(mod) {};

    std::string GetName() override { return "speed"; };
    std::string GetAlias() override { return ""; };
    std::string GetDescription() override { return "Change Realtime Ball Speed."; };
    bool IsCheat() override { return true; };
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override { return {}; };

private:
    BMLMod *m_BMLMod;
};

class CommandTravel : public ICommand {
public:
    explicit CommandTravel(BMLMod *mod) : m_BMLMod(mod) {};

    std::string GetName() override { return "travel"; };
    std::string GetAlias() override { return ""; };
    std::string GetDescription() override { return "Switch to First-Person Camera."; };
    bool IsCheat() override { return false; };
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override { return {}; };

private:
    BMLMod *m_BMLMod;
};

#endif // BML_COMMANDS_H