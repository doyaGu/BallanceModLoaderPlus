#ifndef BML_HUD_COMMAND_H
#define BML_HUD_COMMAND_H

#include "BML/ICommand.h"

class HUDRuntime;

class HUDCommand : public ICommand {
public:
    explicit HUDCommand(HUDRuntime *hud);

    std::string GetName() override { return "hud"; }
    std::string GetAlias() override { return ""; }
    std::string GetDescription() override { return "Commands for HUD."; }
    bool IsCheat() override { return false; }
    void Execute(IBML *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override;

private:
    HUDRuntime *m_HUD;
    int m_State;
};

#endif // BML_HUD_COMMAND_H
