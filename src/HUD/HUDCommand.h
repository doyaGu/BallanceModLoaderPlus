#ifndef BML_HUD_COMMAND_H
#define BML_HUD_COMMAND_H

#include "Console/ICommand.h"

class HUDRuntime;

class HUDCommand : public ICommand {
public:
    explicit HUDCommand(HUDRuntime *hud);

    std::string GetName() override { return "hud"; }
    std::string GetAlias() override { return ""; }
    std::string GetDescription() override { return "Commands for HUD."; }
    bool IsCheat() override { return false; }
    void Execute(ModContext *bml, const std::vector<std::string> &args) override;
    const std::vector<std::string> GetTabCompletion(ModContext *bml, const std::vector<std::string> &args) override;

private:
    HUDRuntime *m_HUD;
    int m_State;
};

#endif // BML_HUD_COMMAND_H
