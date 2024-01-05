#pragma once

#include <map>

#include "BML/BMLAll.h"

class DebugUtilities;

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
    explicit CommandKill(DebugUtilities *mod) : m_Mod(mod) {}

    std::string GetName() override { return "kill"; };
    std::string GetAlias() override { return ""; };
    std::string GetDescription() override { return "Suicide."; };
    bool IsCheat() override { return false; };
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override { return {}; };

private:
    DebugUtilities *m_Mod;
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
    explicit CommandSector(DebugUtilities *mod) : m_Mod(mod) {}

    std::string GetName() override { return "sector"; };
    std::string GetAlias() override { return ""; };
    std::string GetDescription() override { return "Start playing specified sector."; };
    bool IsCheat() override { return true; };
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override { return {}; };

private:
    DebugUtilities *m_Mod;
};

class CommandSpeed : public ICommand {
public:
    explicit CommandSpeed(DebugUtilities *mod) : m_Mod(mod) {}

    std::string GetName() override { return "speed"; };
    std::string GetAlias() override { return ""; };
    std::string GetDescription() override { return "Change Realtime Ball Speed."; };
    bool IsCheat() override { return true; };
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override { return {}; };

private:
    DebugUtilities *m_Mod;
};