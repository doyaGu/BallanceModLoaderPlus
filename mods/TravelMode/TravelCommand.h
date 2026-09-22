#pragma once

#include "BML/BMLAll.h"

class TravelMode;

class TravelCommand : public ICommand {
public:
    explicit TravelCommand(class TravelMode *mod) : m_Mod(mod) {};

    std::string GetName() override { return "travel"; };
    std::string GetAlias() override { return ""; };
    std::string GetDescription() override { return "Switch to First-Person Camera."; };
    bool IsCheat() override { return false; };
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override { return {}; };

private:
    TravelMode *m_Mod;
};
