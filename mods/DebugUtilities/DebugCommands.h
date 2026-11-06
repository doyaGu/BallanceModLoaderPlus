#pragma once

#include <map>

#include "Common/ModSupport.hpp"
#include "Common/Commands.hpp"

class DebugUtilities;

class CommandScore {
  public:
    static BML::Command::Definition Define() {
        BML::Command::Definition d;
        d.Name = "score";
        d.Alias = "";
        d.Description = "Manage Ingame Score.";
        d.Cheat = true;
        return d;
    }

    void Execute(const BML::Mod::Context &context, const std::vector<std::string> &args);
    const std::vector<std::string> GetTabCompletion(const BML::Mod::Context &context,
                                                    const std::vector<std::string> &args) {
        return args.size() == 2 ? std::vector<std::string>({"add", "sub", "set"})
                                : std::vector<std::string>();
    };

  private:
    ModSupport::Object<CKDataArray> m_Energy{};
};

class CommandKill {
  public:
    static BML::Command::Definition Define() {
        BML::Command::Definition d;
        d.Name = "kill";
        d.Alias = "";
        d.Description = "Suicide.";
        d.Cheat = false;
        return d;
    }

    explicit CommandKill(DebugUtilities *mod) : m_Mod(mod) {}
    void Execute(const BML::Mod::Context &context, const std::vector<std::string> &args);
    const std::vector<std::string> GetTabCompletion(const BML::Mod::Context &context,
                                                    const std::vector<std::string> &args) {
        return {};
    };

  private:
    DebugUtilities *m_Mod;
    ModSupport::Object<CKBehavior> m_DeactivateBall{};
};

class CommandSetSpawn {
  public:
    static BML::Command::Definition Define() {
        BML::Command::Definition d;
        d.Name = "spawn";
        d.Alias = "";
        d.Description = "Set Ball Spawn Point to Current Position.";
        d.Cheat = true;
        return d;
    }

    void Execute(const BML::Mod::Context &context, const std::vector<std::string> &args);
    const std::vector<std::string> GetTabCompletion(const BML::Mod::Context &context,
                                                    const std::vector<std::string> &args) {
        return {};
    };

  private:
    ModSupport::Object<CKDataArray> m_CurLevel{};
};

class CommandWin {
  public:
    static BML::Command::Definition Define() {
        BML::Command::Definition d;
        d.Name = "win";
        d.Alias = "";
        d.Description = "Finish this Level.";
        d.Cheat = true;
        return d;
    }

    void Execute(const BML::Mod::Context &context, const std::vector<std::string> &args);
    const std::vector<std::string> GetTabCompletion(const BML::Mod::Context &context,
                                                    const std::vector<std::string> &args) {
        return {};
    };
};

class CommandSector {
  public:
    static BML::Command::Definition Define() {
        BML::Command::Definition d;
        d.Name = "sector";
        d.Alias = "";
        d.Description = "Start playing specified sector.";
        d.Cheat = true;
        return d;
    }

    explicit CommandSector(DebugUtilities *mod) : m_Mod(mod) {}
    void Execute(const BML::Mod::Context &context, const std::vector<std::string> &args);
    const std::vector<std::string> GetTabCompletion(const BML::Mod::Context &context,
                                                    const std::vector<std::string> &args) {
        return {};
    };

  private:
    DebugUtilities *m_Mod;
};

class CommandSpeed {
  public:
    static BML::Command::Definition Define() {
        BML::Command::Definition d;
        d.Name = "speed";
        d.Alias = "";
        d.Description = "Change Realtime Ball Speed.";
        d.Cheat = true;
        return d;
    }

    explicit CommandSpeed(DebugUtilities *mod) : m_Mod(mod) {}
    void Execute(const BML::Mod::Context &context, const std::vector<std::string> &args);
    const std::vector<std::string> GetTabCompletion(const BML::Mod::Context &context,
                                                    const std::vector<std::string> &args) {
        return {};
    };

  private:
    DebugUtilities *m_Mod;
};