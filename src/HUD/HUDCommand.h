#ifndef BML_HUD_COMMAND_H
#define BML_HUD_COMMAND_H

#include "Console/CommandRecord.h"

class HUDRuntime;

class HUDCommand {
public:
    explicit HUDCommand(HUDRuntime *hud);

    std::string GetName() { return "hud"; }

    std::string GetAlias() { return ""; }

    std::string GetDescription() { return "Commands for HUD."; }

    bool IsCheat() { return false; }

    void Execute(ModContext *bml, const std::vector<std::string> &args);
    const std::vector<std::string> GetTabCompletion(ModContext *bml,
                                                    const std::vector<std::string> &args);

private:
    HUDRuntime *m_HUD;
    int m_State;
};

#endif // BML_HUD_COMMAND_H
