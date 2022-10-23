#ifndef BML_COMMANDS_H
#define BML_COMMANDS_H

#include <map>

#include "CKAll.h"

#include "ICommand.h"

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
    std::string GetName() override { return "clear"; };
    std::string GetAlias() override { return ""; };
    std::string GetDescription() override { return "Clear the Console."; };
    bool IsCheat() override { return false; };
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override { return {}; };
};

class CommandScore : public ICommand {
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

class CommandSpeed : public ICommand {
    std::string GetName() override { return "speed"; };
    std::string GetAlias() override { return ""; };
    std::string GetDescription() override { return "Change Realtime Ball Speed."; };
    bool IsCheat() override { return true; };
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override { return {}; };

private:
    CKDataArray *m_CurLevel = nullptr, *m_PhysicsBall = nullptr;
    CKParameter *m_Force = nullptr;
    std::map<std::string, float> m_Forces;
};

class CommandKill : public ICommand {
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
    std::string GetName() override { return "spawn"; };
    std::string GetAlias() override { return ""; };
    std::string GetDescription() override { return "Set Ball Spawn Point to Current Position."; };
    bool IsCheat() override { return true; };
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override { return {}; };

private:
    CKDataArray *m_CurLevel = nullptr;
};

class CommandSector : public ICommand {
    std::string GetName() override { return "sector"; };
    std::string GetAlias() override { return ""; };
    std::string GetDescription() override { return "Start playing specified sector."; };
    bool IsCheat() override { return true; };
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override { return {}; };

    void ResetBall(IBML *bml, CKContext *ctx);

private:
    CKDataArray *m_CurLevel = nullptr, *m_Checkpoints = nullptr, *m_ResetPoints = nullptr, *m_IngameParam = nullptr;
    CKParameter *m_CurSector = nullptr;
};

class CommandWin : public ICommand {
    std::string GetName() override { return "win"; };
    std::string GetAlias() override { return ""; };
    std::string GetDescription() override { return "Finish this Level."; };
    bool IsCheat() override { return true; };
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override { return {}; };
};

class CommandTravel : public ICommand {
public:
    explicit CommandTravel(class BMLMod *mod) : m_BMLMod(mod) {};

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