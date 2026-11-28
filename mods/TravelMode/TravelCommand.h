#pragma once

#include "Common/ModSupport.hpp"
#include "Common/Commands.hpp"

class TravelMode;

class TravelCommand {
  public:
    static BML::Command::Definition Define() {
        BML::Command::Definition d;
        d.Name = "travel";
        d.Alias = "";
        d.Description = "Switch to First-Person Camera.";
        d.Cheat = false;
        return d;
    }

    explicit TravelCommand(class TravelMode *mod) : m_Mod(mod) {};
    void Execute(const BML::Mod::Context &context, const std::vector<std::string> &args);
    const std::vector<std::string> GetTabCompletion(const BML::Mod::Context &context,
                                                    const std::vector<std::string> &args) {
        return {};
    };

  private:
    TravelMode *m_Mod;
};
